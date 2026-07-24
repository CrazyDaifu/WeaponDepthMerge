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
	}

	~device_state()
	{
		release_resources();
	}

	void release_resources()
	{
		if (combined_surface != nullptr)
		{
			combined_surface->Release();
			combined_surface = nullptr;
		}

		if (scratch_dsv != nullptr)
		{
			scratch_dsv->Release();
			scratch_dsv = nullptr;
		}

		combined_dsv = { 0 };
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

	resource_view current_dsv = { 0 };
	viewport current_viewport = {};

	resource_view combined_dsv = { 0 };
	IDirect3DSurface9 *combined_surface = nullptr;
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

	// ReShade may emit a stale resource-view handle around D3D9 Reset. Query the
	// currently bound native surface instead of dereferencing the event handle.
	IDirect3DSurface9 *surface = nullptr;
	if (state.native_device == nullptr || FAILED(state.native_device->GetDepthStencilSurface(&surface)) || surface == nullptr)
		return false;

	D3DSURFACE_DESC desc = {};
	constexpr D3DFORMAT intz_format = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
	if (FAILED(surface->GetDesc(&desc)) ||
		desc.MultiSampleType != D3DMULTISAMPLE_NONE ||
		desc.Format != intz_format ||
		desc.Width <= 512 ||
		!viewport_matches(state.current_viewport, desc.Width, desc.Height))
	{
		surface->Release();
		return false;
	}

	state.combined_dsv = dsv;
	state.combined_surface = surface;
	state.combined_width = desc.Width;
	state.combined_height = desc.Height;
	state.candidate_format = format::intz;

	char message[160];
	std::snprintf(message, sizeof(message), "Weapon Depth Merge selected an INTZ depth buffer (%ux%u).", state.combined_width, state.combined_height);
	reshade::log::message(reshade::log::level::info, message);
	return true;
}

static bool is_target_draw(device_state &state)
{
	if (!s_enabled || state.primitive_up_pending || state.current_dsv == 0)
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

	if (!create_scratch_depth(state))
	{
		++state.failed_drawcalls;
		return false;
	}

	IDirect3DSurface9 *const combined_surface = state.combined_surface;
	if (combined_surface == nullptr)
		return false;

	D3DVIEWPORT9 original_viewport = {};
	DWORD color_write_masks[4] = {};
	DWORD stencil_write_mask = 0;

	state.native_device->GetViewport(&original_viewport);
	state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE, &color_write_masks[0]);
	state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE1, &color_write_masks[1]);
	state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE2, &color_write_masks[2]);
	state.native_device->GetRenderState(D3DRS_COLORWRITEENABLE3, &color_write_masks[3]);
	state.native_device->GetRenderState(D3DRS_STENCILWRITEMASK, &stencil_write_mask);

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
		state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE, color_write_masks[0]);
		state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE1, color_write_masks[1]);
		state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE2, color_write_masks[2]);
		state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE3, color_write_masks[3]);
		state.native_device->SetRenderState(D3DRS_STENCILWRITEMASK, stencil_write_mask);
		state.native_device->SetViewport(&original_viewport);
		state.native_device->SetDepthStencilSurface(combined_surface);
		return false;
	}
	draw_call();

	// Then execute the game's normal draw against the clearable scratch depth.
	state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE, color_write_masks[0]);
	state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE1, color_write_masks[1]);
	state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE2, color_write_masks[2]);
	state.native_device->SetRenderState(D3DRS_COLORWRITEENABLE3, color_write_masks[3]);
	state.native_device->SetRenderState(D3DRS_STENCILWRITEMASK, stencil_write_mask);
	state.native_device->SetViewport(&original_viewport);
	if (FAILED(state.native_device->SetDepthStencilSurface(state.scratch_dsv)))
	{
		++state.failed_drawcalls;
		state.native_device->SetDepthStencilSurface(combined_surface);
		return true;
	}
	draw_call();

	// Keep the application-visible replacement bound, like old ReShade did internally.
	state.native_device->SetDepthStencilSurface(combined_surface);

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
	}
}

static void on_destroy_command_list(command_list *cmd_list)
{
	if (cmd_list == nullptr || cmd_list->get_device()->get_api() != device_api::d3d9)
		return;

	if (device_state *const state = get_state(cmd_list))
		state->release_resources();
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
		state != nullptr && view == state->combined_dsv)
	{
		state->current_dsv = { 0 };
		state->release_resources();
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
	if (!s_enabled || dsv == 0)
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
	state.native_device->GetDepthStencilSurface(&previous_dsv);
	state.native_device->SetDepthStencilSurface(state.scratch_dsv);

	DWORD clear_flags = 0;
	if (depth != nullptr)
		clear_flags |= D3DCLEAR_ZBUFFER;
	if (stencil != nullptr)
		clear_flags |= D3DCLEAR_STENCIL;

	state.native_device->Clear(
		rect_count,
		reinterpret_cast<const D3DRECT *>(rects),
		clear_flags,
		0,
		depth != nullptr ? *depth : 1.0f,
		stencil != nullptr ? *stencil : 0);

	state.native_device->SetDepthStencilSurface(previous_dsv);
	if (previous_dsv != nullptr)
		previous_dsv->Release();

	return true;
}

static void on_present(command_queue *, swapchain *swapchain, const rect *, const rect *, uint32_t, const rect *)
{
	if (swapchain == nullptr || swapchain->get_device()->get_api() != device_api::d3d9)
		return;

	if (device_state *const state = swapchain->get_device()->get_private_data<device_state>())
		state->reset_frame();
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
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Weapon Depth Merge 1.1 RC17 without D3D9 bind-vertex-buffer events for ReShade 6.7.3.";

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(module))
			return FALSE;

		reshade::register_event<reshade::addon_event::init_device>(on_init_device);
		reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
		reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
		reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
		reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
		reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets);
		reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
		reshade::register_event<reshade::addon_event::present>(on_present);
		break;

	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(module);
		break;
	}

	return TRUE;
}
