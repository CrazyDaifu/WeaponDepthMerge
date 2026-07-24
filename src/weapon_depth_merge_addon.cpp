/*
 * Weapon Depth Merge for ReShade 6.7.3
 * D3D9 x86 prototype, initially targeting Battlefield 2.
 *
 * This implementation is based only on the official ReShade 6.7.3 API and
 * the verified Thalixte ReShade implementation at commit 6dbe6c6d8fb45af8af11a53f53dd67043c809c23.
 */

#include <imgui.h>
#include <reshade.hpp>

#include <d3d9.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace reshade::api;

namespace
{
constexpr uint32_t min_segment_drawcalls = 4;
constexpr uint64_t min_segment_vertices = 20;

bool s_enabled = true;
uint32_t s_forced_clear_index = 1;
float s_depth_bias = 0.5f;
bool s_require_full_size_viewport = true;

struct segment_stats
{
	uint32_t drawcalls = 0;
	uint64_t vertices = 0;
};

struct __declspec(uuid("9fd929d7-1c89-4efc-93ae-36851155b324")) device_state
{
	explicit device_state(device *owner) : owner(owner)
	{
		native_device = reinterpret_cast<IDirect3DDevice9 *>(owner->get_native());

		D3DDEVICE_CREATION_PARAMETERS creation_parameters = {};
		if (native_device != nullptr && SUCCEEDED(native_device->GetCreationParameters(&creation_parameters)))
			focus_window = creation_parameters.hFocusWindow;
	}

	~device_state()
	{
		release_resources();
	}

	void release_resources(resource_view destroyed_view = { 0 })
	{
		if (combined_srv != 0 && combined_srv != destroyed_view)
		{
			owner->destroy_resource_view(combined_srv);
		}
		combined_srv = { 0 };
		bound_depth_srv = { 0 };

		if (scratch_dsv != nullptr)
		{
			scratch_dsv->Release();
			scratch_dsv = nullptr;
		}

		combined_dsv = { 0 };
		combined_resource = { 0 };
		combined_width = 0;
		combined_height = 0;
		candidate_format = format::unknown;
		weapon_phase = false;
	}

	void reset_frame()
	{
		if (current_segment.drawcalls > min_segment_drawcalls && current_segment.vertices > min_segment_vertices)
			frame_segments.push_back(current_segment);

		last_segments = frame_segments;
		frame_segments.clear();
		current_segment = {};
		meaningful_clear_count = 0;
		weapon_phase = false;
		primitive_up_pending = false;

		uint64_t best_vertices = 0;
		auto_clear_index = 1;
		for (uint32_t i = 0; i < last_segments.size(); ++i)
		{
			if (last_segments[i].vertices >= best_vertices)
			{
				best_vertices = last_segments[i].vertices;
				auto_clear_index = i + 1;
			}
		}
	}

	uint32_t active_clear_index() const
	{
		return s_forced_clear_index != 0 ? s_forced_clear_index : auto_clear_index;
	}

	device *owner = nullptr;
	IDirect3DDevice9 *native_device = nullptr;
	HWND focus_window = nullptr;

	resource_view current_dsv = { 0 };
	viewport current_viewport = {};

	resource_view combined_dsv = { 0 };
	resource combined_resource = { 0 };
	resource_view combined_srv = { 0 };
	resource_view bound_depth_srv = { 0 };
	IDirect3DSurface9 *scratch_dsv = nullptr;
	uint32_t combined_width = 0;
	uint32_t combined_height = 0;
	format candidate_format = format::unknown;

	segment_stats current_segment;
	std::vector<segment_stats> frame_segments;
	std::vector<segment_stats> last_segments;
	uint32_t meaningful_clear_count = 0;
	uint32_t auto_clear_index = 1;
	bool weapon_phase = false;
	bool primitive_up_pending = false;
	bool suspended = false;
	bool focus_paused = false;
	bool effects_reload_pending = false;
	uint32_t effects_reload_cooldown = 0;
	uint64_t merged_drawcalls = 0;
	uint64_t skipped_up_drawcalls = 0;
	uint64_t failed_drawcalls = 0;
};

static device_state *get_state(command_list *cmd_list)
{
	if (cmd_list == nullptr || cmd_list->get_device()->get_api() != device_api::d3d9)
		return nullptr;

	return cmd_list->get_device()->get_private_data<device_state>();
}

static bool is_device_ready(device_state &state)
{
	if (state.suspended || state.native_device == nullptr)
		return false;

	if (FAILED(state.native_device->TestCooperativeLevel()))
	{
		state.suspended = true;
		return false;
	}

	return true;
}

static bool is_application_foreground(const device_state &state)
{
	if (state.focus_window == nullptr)
		return true;

	const HWND foreground_window = GetForegroundWindow();
	if (foreground_window == nullptr)
		return false;

	DWORD focus_process_id = 0;
	DWORD foreground_process_id = 0;
	GetWindowThreadProcessId(state.focus_window, &focus_process_id);
	GetWindowThreadProcessId(foreground_window, &foreground_process_id);
	return focus_process_id != 0 && focus_process_id == foreground_process_id;
}

static bool is_interception_ready(device_state &state)
{
	if (!is_application_foreground(state))
	{
		state.focus_paused = true;
		return false;
	}

	// Do not re-enable interception after a focus loss. A D3D9 reset can leave
	// application and ReShade resources in different generations until restart.
	return !state.focus_paused && is_device_ready(state);
}

static bool viewport_matches(const viewport &viewport, uint32_t width, uint32_t height)
{
	if (!s_require_full_size_viewport)
		return true;

	if (viewport.width <= 0.0f || viewport.height <= 0.0f)
		return false;

	return std::fabs(viewport.width - static_cast<float>(width)) <= static_cast<float>(width) * 0.05f &&
		std::fabs(viewport.height - static_cast<float>(height)) <= static_cast<float>(height) * 0.05f;
}

static bool create_scratch_depth(device_state &state)
{
	if (state.scratch_dsv != nullptr)
		return true;

	if (state.native_device == nullptr || state.combined_width == 0 || state.combined_height == 0)
		return false;

	constexpr D3DFORMAT formats[] = {
		D3DFMT_D24S8,
		D3DFMT_D24X8,
		D3DFMT_D16,
	};

	for (const D3DFORMAT depth_format : formats)
	{
		if (SUCCEEDED(state.native_device->CreateDepthStencilSurface(
			state.combined_width,
			state.combined_height,
			depth_format,
			D3DMULTISAMPLE_NONE,
			0,
			FALSE,
			&state.scratch_dsv,
			nullptr)))
			return true;
	}

	reshade::log::message(reshade::log::level::error, "Weapon Depth Merge failed to create the scratch D3D9 depth surface.");
	return false;
}

static bool select_combined_depth(device_state &state, resource_view dsv)
{
	if (dsv == 0 || state.combined_dsv != 0)
		return state.combined_dsv == dsv;

	device *const device = state.owner;
	const resource resource = device->get_resource_from_view(dsv);
	if (resource == 0)
		return false;

	const resource_desc desc = device->get_resource_desc(resource);
	if (desc.type != resource_type::texture_2d ||
		desc.texture.samples != 1 ||
		desc.texture.format != format::intz ||
		desc.texture.width <= 512 ||
		!viewport_matches(state.current_viewport, desc.texture.width, desc.texture.height))
		return false;

	resource_view srv = { 0 };
	const resource_view_desc srv_desc(resource_view_type::texture_2d, format_to_default_typed(desc.texture.format), 0, 1, 0, 1);
	if (!device->create_resource_view(resource, resource_usage::shader_resource, srv_desc, &srv))
	{
		reshade::log::message(reshade::log::level::error, "Weapon Depth Merge found an INTZ depth buffer, but failed to create its shader resource view.");
		return false;
	}

	state.combined_dsv = dsv;
	state.combined_resource = resource;
	state.combined_srv = srv;
	state.combined_width = desc.texture.width;
	state.combined_height = desc.texture.height;
	state.candidate_format = desc.texture.format;

	char message[160];
	std::snprintf(message, sizeof(message), "Weapon Depth Merge selected an INTZ depth buffer (%ux%u).", state.combined_width, state.combined_height);
	reshade::log::message(reshade::log::level::info, message);
	return true;
}

static bool is_target_draw(device_state &state)
{
	if (!s_enabled || state.suspended || state.focus_paused || state.effects_reload_pending || state.primitive_up_pending || state.current_dsv == 0)
		return false;

	// Resource-view creation is unsafe while D3D9 is resetting during map loading.
	if (!is_interception_ready(state))
		return false;

	if (state.combined_dsv == 0)
		select_combined_depth(state, state.current_dsv);

	return state.combined_dsv != 0 &&
		state.current_dsv == state.combined_dsv &&
		viewport_matches(state.current_viewport, state.combined_width, state.combined_height);
}

template <typename DrawCall>
static bool replay_weapon_draw(command_list *cmd_list, DrawCall &&draw_call)
{
	device_state *const state_ptr = get_state(cmd_list);
	if (state_ptr == nullptr)
		return false;

	device_state &state = *state_ptr;
	if (!is_target_draw(state))
	{
		if (state.primitive_up_pending)
			++state.skipped_up_drawcalls;
		return false;
	}

	if (!state.weapon_phase)
		return false;
	if (!is_interception_ready(state) || state.effects_reload_pending)
		return false;

	if (!create_scratch_depth(state))
	{
		++state.failed_drawcalls;
		return false;
	}

	IDirect3DSurface9 *const combined_surface = reinterpret_cast<IDirect3DSurface9 *>(state.combined_dsv.handle & ~1ull);
	if (combined_surface == nullptr)
		return false;

	D3DVIEWPORT9 original_viewport = {};
	DWORD color_write_masks[4] = {};
	DWORD stencil_write_mask = 0;
	IDirect3DSurface9 *original_dsv = nullptr;

	if (FAILED(state.native_device->GetDepthStencilSurface(&original_dsv)) || original_dsv == nullptr ||
		FAILED(state.native_device->GetViewport(&original_viewport)) ||
		FAILED(state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE, &color_write_masks[0])) ||
		FAILED(state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE1, &color_write_masks[1])) ||
		FAILED(state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE2, &color_write_masks[2])) ||
		FAILED(state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE3, &color_write_masks[3])) ||
		FAILED(state.native_device->GetRenderState(D3DRS_STENCILWRITEMASK, &stencil_write_mask)))
	{
		if (original_dsv != nullptr)
			original_dsv->Release();
		state.suspended = true;
		++state.failed_drawcalls;
		return false;
	}

	auto restore_render_state = [&]() {
		bool success = true;
		success &= SUCCEEDED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE, color_write_masks[0]));
		success &= SUCCEEDED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE1, color_write_masks[1]));
		success &= SUCCEEDED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE2, color_write_masks[2]));
		success &= SUCCEEDED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE3, color_write_masks[3]));
		success &= SUCCEEDED(state.native_device->SetRenderState(D3DRS_STENCILWRITEMASK, stencil_write_mask));
		success &= SUCCEEDED(state.native_device->SetViewport(&original_viewport));
		return success;
	};
	auto restore_depth_surface = [&]() {
		return SUCCEEDED(state.native_device->SetDepthStencilSurface(original_dsv));
	};
	auto release_original_depth = [&]() {
		original_dsv->Release();
		original_dsv = nullptr;
	};

	// Match Thalixte's order: write biased first-person geometry into preserved depth first.
	D3DVIEWPORT9 biased_viewport = original_viewport;
	biased_viewport.MinZ -= s_depth_bias;
	biased_viewport.MaxZ -= s_depth_bias;

	if (FAILED(state.native_device->SetDepthStencilSurface(combined_surface)) ||
		FAILED(state.native_device->SetViewport(&biased_viewport)) ||
		FAILED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE, 0)) ||
		FAILED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE1, 0)) ||
		FAILED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE2, 0)) ||
		FAILED(state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE3, 0)) ||
		FAILED(state.native_device->SetRenderState(D3DRS_STENCILWRITEMASK, 0)))
	{
		++state.failed_drawcalls;
		state.suspended = true;
		restore_render_state();
		restore_depth_surface();
		release_original_depth();
		return false;
	}
	draw_call();

	// Then execute the game's normal draw against the clearable scratch depth.
	if (!restore_render_state())
	{
		++state.failed_drawcalls;
		state.suspended = true;
		restore_depth_surface();
		release_original_depth();
		return false;
	}
	if (FAILED(state.native_device->SetDepthStencilSurface(state.scratch_dsv)))
	{
		++state.failed_drawcalls;
		state.suspended = true;
		restore_depth_surface();
		release_original_depth();
		return false;
	}
	draw_call();

	// Restore the exact surface that was bound before the replay, since focus changes can invalidate tracked assumptions.
	if (!restore_depth_surface())
	{
		++state.failed_drawcalls;
		state.suspended = true;
		release_original_depth();
		return true; // The normal draw already ran, so do not execute it a second time.
	}
	release_original_depth();

	++state.merged_drawcalls;
	return true;
}

static void on_init_device(device *device)
{
	if (device != nullptr && device->get_api() == device_api::d3d9)
	{
		device->create_private_data<device_state>(device);
	}
}

static void on_destroy_device(device *device)
{
	if (device != nullptr && device->get_api() == device_api::d3d9 && device->get_private_data<device_state>() != nullptr)
	{
		device->destroy_private_data<device_state>();
	}
}

static void on_init_command_list(command_list *cmd_list)
{
	if (cmd_list == nullptr || cmd_list->get_device()->get_api() != device_api::d3d9)
		return;

	if (device_state *const state = get_state(cmd_list))
	{
		state->current_dsv = { 0 };
		state->current_viewport = {};
		state->suspended = false;
	}
}

static void on_destroy_command_list(command_list *cmd_list)
{
	if (cmd_list == nullptr || cmd_list->get_device()->get_api() != device_api::d3d9)
		return;

	if (device_state *const state = get_state(cmd_list))
	{
		state->suspended = true;
		state->release_resources();
	}
}

static void on_bind_render_targets(command_list *cmd_list, uint32_t, const resource_view *, resource_view dsv)
{
	if (device_state *const state = get_state(cmd_list))
		state->current_dsv = dsv;
}

static void on_destroy_resource_view(device *device, resource_view view)
{
	if (device == nullptr || device->get_api() != device_api::d3d9)
		return;

	if (device_state *const state = device->get_private_data<device_state>();
		state != nullptr && (view == state->combined_dsv || view == state->combined_srv))
	{
		state->current_dsv = { 0 };
		state->release_resources(view);
	}
}

static void on_bind_viewports(command_list *cmd_list, uint32_t first, uint32_t count, const viewport *viewports)
{
	if (first != 0 || count == 0)
		return;

	if (device_state *const state = get_state(cmd_list))
		state->current_viewport = viewports[0];
}

static void on_bind_vertex_buffers(command_list *cmd_list, uint32_t first, uint32_t count, const resource *buffers, const uint64_t *, const uint32_t *)
{
	if (first != 0 || count == 0)
		return;

	device_state *const state = get_state(cmd_list);
	if (state == nullptr)
		return;

	if (buffers[0] == 0)
	{
		state->primitive_up_pending = false;
		return;
	}

	IDirect3DVertexBuffer9 *actual_stream = nullptr;
	UINT offset = 0, stride = 0;
	state->primitive_up_pending = false;
	if (SUCCEEDED(state->native_device->GetStreamSource(0, &actual_stream, &offset, &stride)))
	{
		state->primitive_up_pending = reinterpret_cast<uint64_t>(actual_stream) != buffers[0].handle;
		if (actual_stream != nullptr)
			actual_stream->Release();
	}
}

static bool on_draw(command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	device_state *const state_ptr = get_state(cmd_list);
	if (state_ptr == nullptr)
		return false;

	device_state &state = *state_ptr;
	const bool target_draw = is_target_draw(state);
	if (target_draw)
	{
		state.current_segment.vertices += static_cast<uint64_t>(vertices) * instances;
		++state.current_segment.drawcalls;
	}
	else if (state.primitive_up_pending)
	{
		++state.skipped_up_drawcalls;
		return false;
	}

	return replay_weapon_draw(cmd_list, [=]() {
		cmd_list->draw(vertices, instances, first_vertex, first_instance);
	});
}

static bool on_draw_indexed(command_list *cmd_list, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	device_state *const state_ptr = get_state(cmd_list);
	if (state_ptr == nullptr)
		return false;

	device_state &state = *state_ptr;
	const bool target_draw = is_target_draw(state);
	if (target_draw)
	{
		state.current_segment.vertices += static_cast<uint64_t>(indices) * instances;
		++state.current_segment.drawcalls;
	}
	else if (state.primitive_up_pending)
	{
		++state.skipped_up_drawcalls;
		return false;
	}

	return replay_weapon_draw(cmd_list, [=]() {
		cmd_list->draw_indexed(indices, instances, first_index, vertex_offset, first_instance);
	});
}

static bool on_clear_depth(command_list *cmd_list, resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const rect *rects)
{
	device_state *const state_ptr = get_state(cmd_list);
	if (state_ptr == nullptr)
		return false;

	device_state &state = *state_ptr;
	if (!s_enabled || dsv == 0 || !is_interception_ready(state))
		return false;

	if (state.combined_dsv == 0)
		select_combined_depth(state, dsv);

	if (dsv != state.combined_dsv)
		return false;

	bool selected_boundary = false;
	if (state.current_segment.drawcalls > min_segment_drawcalls && state.current_segment.vertices > min_segment_vertices)
	{
		state.frame_segments.push_back(state.current_segment);
		state.current_segment = {};
		++state.meaningful_clear_count;
		selected_boundary = state.meaningful_clear_count == state.active_clear_index();
	}

	if (!state.weapon_phase && !selected_boundary)
		return false;

	if (!create_scratch_depth(state))
		return false;

	state.weapon_phase = true;

	IDirect3DSurface9 *previous_dsv = nullptr;
	if (FAILED(state.native_device->GetDepthStencilSurface(&previous_dsv)) ||
		FAILED(state.native_device->SetDepthStencilSurface(state.scratch_dsv)))
	{
		if (previous_dsv != nullptr)
			previous_dsv->Release();
		state.suspended = true;
		++state.failed_drawcalls;
		return false;
	}

	DWORD clear_flags = 0;
	if (depth != nullptr)
		clear_flags |= D3DCLEAR_ZBUFFER;
	if (stencil != nullptr)
		clear_flags |= D3DCLEAR_STENCIL;

	const HRESULT clear_result = state.native_device->Clear(
		rect_count,
		reinterpret_cast<const D3DRECT *>(rects),
		clear_flags,
		0,
		depth != nullptr ? *depth : 1.0f,
		stencil != nullptr ? *stencil : 0);

	const HRESULT restore_result = state.native_device->SetDepthStencilSurface(previous_dsv);
	if (previous_dsv != nullptr)
		previous_dsv->Release();

	if (FAILED(clear_result) || FAILED(restore_result))
	{
		state.suspended = true;
		++state.failed_drawcalls;
		return false;
	}

	return true;
}

static void update_depth_binding(effect_runtime *runtime)
{
	if (runtime == nullptr || runtime->get_device()->get_api() != device_api::d3d9)
		return;

	device_state *const state = runtime->get_device()->get_private_data<device_state>();
	if (state == nullptr)
		return;

	// Once focus is lost or the device is suspended, leave ReShade's existing
	// descriptors untouched. Calling update_texture_bindings during a reset or
	// a second effect reload can race ReShade's descriptor rebuild.
	if (state->suspended || state->focus_paused || state->effects_reload_pending || state->combined_srv == 0)
		return;

	const resource_view srv = state->combined_srv;
	if (state->bound_depth_srv == srv)
		return;
	state->bound_depth_srv = srv;
	runtime->update_texture_bindings("DEPTH", srv, srv);
}

static void on_reloaded_effects(effect_runtime *runtime)
{
	if (runtime == nullptr || runtime->get_device()->get_api() != device_api::d3d9)
		return;

	if (device_state *const state = runtime->get_device()->get_private_data<device_state>())
	{
		// ReShade emits this before and after rebuilding effects. Keep the add-on
		// inert for a few complete effect frames so descriptor rebuilding can settle.
		state->effects_reload_pending = true;
		state->effects_reload_cooldown = 120;
		state->bound_depth_srv = { 0 };
	}
}

static void on_begin_effects(effect_runtime *runtime, command_list *cmd_list, resource_view, resource_view)
{
	if (runtime == nullptr || cmd_list == nullptr || runtime->get_device()->get_api() != device_api::d3d9)
		return;

	device_state *const state = runtime->get_device()->get_private_data<device_state>();
	if (state == nullptr)
		return;

	if (state->effects_reload_pending)
	{
		if (state->effects_reload_cooldown != 0)
		{
			--state->effects_reload_cooldown;
			return;
		}

		state->effects_reload_pending = false;
		state->bound_depth_srv = { 0 };
	}

	const bool device_ready = is_interception_ready(*state);
	update_depth_binding(runtime);
	if (!device_ready || state->combined_srv == 0)
		return;

	// D3D9 cannot sample an INTZ texture while its surface is still bound as depth output.
	cmd_list->bind_render_targets_and_depth_stencil(0, nullptr);
}

static void on_present(command_queue *, swapchain *swapchain, const rect *, const rect *, uint32_t, const rect *)
{
	if (swapchain == nullptr || swapchain->get_device()->get_api() != device_api::d3d9)
		return;

	if (device_state *const state = swapchain->get_device()->get_private_data<device_state>())
	{
		if (!is_application_foreground(*state))
		{
			state->focus_paused = true;
		}
		state->reset_frame();
	}
}

static void draw_settings(effect_runtime *runtime)
{
	if (runtime == nullptr || runtime->get_device()->get_api() != device_api::d3d9)
	{
		ImGui::TextUnformatted("Weapon Depth Merge 1.1 is inactive: this application is not using native D3D9.");
		ImGui::TextWrapped("DXVK uses Vulkan. The add-on now loads safely there, but depth merging remains available only with native D3D9.");
		return;
	}

	device_state *const state = runtime->get_device()->get_private_data<device_state>();
	if (state == nullptr)
	{
		ImGui::TextUnformatted("Weapon Depth Merge is available only in D3D9.");
		return;
	}

	if (ImGui::Checkbox("Enable weapon/cockpit depth merge", &s_enabled))
		reshade::set_config_value(nullptr, "WEAPON_DEPTH_MERGE", "Enabled", s_enabled);

	int clear_index = static_cast<int>(s_forced_clear_index);
	if (ImGui::InputInt("Clear index (0 = automatic)", &clear_index))
	{
		s_forced_clear_index = static_cast<uint32_t>(std::max(clear_index, 0));
		reshade::set_config_value(nullptr, "WEAPON_DEPTH_MERGE", "ClearIndex", s_forced_clear_index);
	}

	if (ImGui::SliderFloat("First-person depth bias", &s_depth_bias, 0.0f, 1.0f, "%.3f"))
		reshade::set_config_value(nullptr, "WEAPON_DEPTH_MERGE", "DepthBias", s_depth_bias);

	if (ImGui::Checkbox("Require full-size viewport", &s_require_full_size_viewport))
		reshade::set_config_value(nullptr, "WEAPON_DEPTH_MERGE", "RequireFullSizeViewport", s_require_full_size_viewport);

	ImGui::Separator();
	if (state->combined_dsv != 0)
		ImGui::Text("Combined INTZ: %ux%u", state->combined_width, state->combined_height);
	else
		ImGui::TextUnformatted("Combined INTZ: not found");

	ImGui::Text("Active clear index: %u%s", state->active_clear_index(), s_forced_clear_index == 0 ? " (automatic)" : " (manual)");
	ImGui::Text("Merged draw calls: %llu", static_cast<unsigned long long>(state->merged_drawcalls));
	ImGui::Text("Skipped UP draw calls: %llu", static_cast<unsigned long long>(state->skipped_up_drawcalls));
	ImGui::Text("Failed intercepted draws: %llu", static_cast<unsigned long long>(state->failed_drawcalls));
	ImGui::Text("Device interception: %s", state->suspended ? "suspended until reset" : "active");
	ImGui::Text("Focus interception: %s", state->focus_paused ? "paused until stable" : "active");

	ImGui::Separator();
	ImGui::TextUnformatted("Last frame clear segments:");
	if (state->last_segments.empty())
		ImGui::TextUnformatted("  No meaningful depth clear segments detected.");

	for (uint32_t i = 0; i < state->last_segments.size(); ++i)
	{
		const segment_stats &segment = state->last_segments[i];
		ImGui::Text("%c CLEAR %2u | %5u draws | %10llu vertices",
			(i + 1 == state->active_clear_index()) ? '>' : ' ',
			i + 1,
			segment.drawcalls,
			static_cast<unsigned long long>(segment.vertices));
	}

	ImGui::Separator();
	ImGui::TextWrapped("For this prototype, keep Generic Depth enabled but turn off its 'Copy depth buffer before clear operations' option. Disable in-game MSAA.");
}

static void load_config()
{
	reshade::get_config_value(nullptr, "WEAPON_DEPTH_MERGE", "Enabled", s_enabled);
	reshade::get_config_value(nullptr, "WEAPON_DEPTH_MERGE", "ClearIndex", s_forced_clear_index);
	reshade::get_config_value(nullptr, "WEAPON_DEPTH_MERGE", "DepthBias", s_depth_bias);
	reshade::get_config_value(nullptr, "WEAPON_DEPTH_MERGE", "RequireFullSizeViewport", s_require_full_size_viewport);
}

}

extern "C" __declspec(dllexport) const char *NAME = "Weapon Depth Merge";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Weapon Depth Merge 1.1 RC6 for native D3D9 x86 and ReShade 6.7.3.";

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(module))
			return FALSE;

		load_config();
		reshade::register_event<reshade::addon_event::init_device>(on_init_device);
		reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
		reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
		reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
		reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
		reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets);
		reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
		reshade::register_event<reshade::addon_event::bind_vertex_buffers>(on_bind_vertex_buffers);
		reshade::register_event<reshade::addon_event::draw>(on_draw);
		reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
		reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth);
		reshade::register_event<reshade::addon_event::present>(on_present);
		reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reloaded_effects);
		reshade::register_overlay(nullptr, draw_settings);
		break;

	case DLL_PROCESS_DETACH:
		reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
		reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
		reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
		reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
		reshade::unregister_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
		reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets);
		reshade::unregister_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
		reshade::unregister_event<reshade::addon_event::bind_vertex_buffers>(on_bind_vertex_buffers);
		reshade::unregister_event<reshade::addon_event::draw>(on_draw);
		reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
		reshade::unregister_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth);
		reshade::unregister_event<reshade::addon_event::present>(on_present);
		reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(on_begin_effects);
		reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(on_reloaded_effects);
		reshade::unregister_overlay(nullptr, draw_settings);
		reshade::unregister_addon(module);
		break;
	}

	return TRUE;
}
