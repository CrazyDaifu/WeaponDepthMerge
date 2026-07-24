# ReShade 项目管理与 GitHub 一键发布流程

本文从 WeaponDepthMerge 的实际工程流程中提炼，用作其他 ReShade add-on、插件或修改版项目的仓库模板。项目特有的渲染逻辑不能直接照搬，但目录组织、版本隔离、离线依赖、二进制验证、GitHub Actions 和一键上传流程可以复用。

## 1. 管理目标

一个可持续维护的 ReShade 项目至少应满足以下要求：

- 下一位维护者或 AI 先读一份移交文档即可理解当前状态。
- 已测试版本不可被后续构建覆盖。
- 源码、依赖、构建脚本和许可证齐全，离线即可编译。
- 本地构建与 GitHub Actions 使用相同的目标架构和配置。
- 每个候选版都有独立 DLL、SHA-256、说明和测试结论。
- 普通推送不会误发正式版；正式 Release 必须由明确标签触发。
- 一键上传前自动重新构建并验证，不上传来源不明或过期 DLL。
- 调试假设、失败方案和实测结果写入文档，而不是只留在聊天记录中。

## 2. 推荐目录结构

```text
ProjectName/
|-- .github/
|   |-- ISSUE_TEMPLATE/
|   `-- workflows/
|       |-- build.yml
|       `-- release.yml
|-- build/
|   |-- ProjectName.addon32
|   `-- SHA256SUMS.txt
|-- docs/
|   |-- ARCHITECTURE.md
|   |-- BUILDING.md
|   |-- BUGS.md
|   |-- PUBLISHING.md
|   |-- TESTING.md
|   `-- ReShade项目管理与GitHub一键发布流程.md
|-- releases/
|   |-- 1.0/
|   |-- 1.1-rc1/
|   `-- README.md
|-- scripts/
|   |-- build-release.ps1
|   |-- verify-binary.ps1
|   |-- package-release.ps1
|   `-- publish-github.ps1
|-- src/
|-- third_party/
|   |-- reshade/
|   |-- imgui/
|   `-- source_archives/
|-- reference_sources/
|-- _项目移交.md
|-- CHANGELOG.md
|-- LICENSE
|-- README.md
|-- SOURCES.md
|-- THIRD_PARTY_NOTICES.md
|-- VERSION
|-- ProjectName.sln
|-- project_name.vcxproj
`-- publish-to-github.cmd
```

### 文件职责

| 文件或目录 | 职责 |
| --- | --- |
| `_项目移交.md` | 当前结论、已验证环境、风险、版本关系、下一步；新维护者第一个阅读 |
| `README.md` | 面向用户的功能、安装、兼容性、限制和文档入口 |
| `VERSION` | 唯一当前版本字符串，例如 `1.1-rc3` 或 `1.1` |
| `CHANGELOG.md` | 每个版本的源码变化和实测状态 |
| `docs/BUGS.md` | 按编号记录症状、根因证据、失败方案、候选修复和最终结论 |
| `docs/TESTING.md` | 固定测试环境、操作顺序、预期结果和回归矩阵 |
| `releases/<version>/` | 冻结的版本 DLL、SHA-256 和版本说明，不被下一版本覆盖 |
| `build/` | 当前开发版本输出，可被重新构建覆盖 |
| `third_party/` | 实际参与编译或离线复现所需依赖及许可证 |
| `reference_sources/` | 只供分析的上游关键源码，不参与编译 |
| `scripts/` | 可重复执行的构建、校验、打包和发布入口 |

## 3. 版本与分支隔离

### 版本号

- 正式版：`1.0`、`1.1`。
- 候选版：`1.1-rc1`、`1.1-rc2`。
- `VERSION` 中带 `-` 的版本一律视为预发布版本。
- DLL 的 `DESCRIPTION` 导出、README、CHANGELOG、归档目录必须与 `VERSION` 同步。

### Git 分支

- `main`：最近一个确认可发布的正式基线。
- `release/1.0`：冻结 1.0 维护线。
- `release/1.1`：1.1 候选开发线。
- `v1.0`、`v1.1`：只指向最终确认的正式提交。

不要给未完成实测的 RC 创建正式标签。失败候选版仍应保留提交和归档，因为失败结论也是后续排查的证据。

### 二进制归档规则

每次候选版构建完成后：

1. 重新构建 `Release/Win32` 或项目指定平台。
2. 验证 PE 架构和 ReShade 必要导出。
3. 生成 SHA-256。
4. 将 DLL 和校验文件复制到 `releases/<VERSION>/`。
5. 在该目录放置 README，说明目的、相对上一版的变化和实测状态。
6. 提交后不再用其他版本覆盖此目录。

`build/` 是当前输出，`releases/` 是冻结证据，两者不能混用。

## 4. 依赖与离线构建

### 依赖原则

- 把实际使用的 ReShade API 头文件放入 `third_party/reshade/include/`。
- 保存对应 ReShade 完整源码压缩包，方便以后核对事件语义和实现细节。
- 若使用 ImGui 头文件或源码，保存确定 commit，并保留许可证。
- 在 `SOURCES.md` 记录上游 URL、版本、commit 和用途。
- 在 `THIRD_PARTY_NOTICES.md` 记录再分发许可证。
- 不依赖构建时在线下载，不依赖未固定版本的 Git submodule。

离线打包不等于把 Visual Studio 工具链打进仓库。仓库应包含源码依赖；Windows SDK、MSBuild 和 MSVC 由构建环境提供。

### 本地构建脚本

`scripts/build-release.ps1` 应只完成确定性的 Release 构建：

```powershell
& $MsBuildPath .\project_name.vcxproj `
  /t:Rebuild `
  /p:Configuration=Release `
  /p:Platform=Win32 `
  /m
```

脚本要求：

- `$ErrorActionPreference = 'Stop'`。
- 检查 MSBuild 是否存在。
- 检查 `$LASTEXITCODE`。
- 输出最终二进制绝对路径。
- 不静默吞掉编译错误。

### 二进制验证脚本

`scripts/verify-binary.ps1` 至少检查：

- 文件存在。
- x86 项目为 `14C machine (x86)`；x64 项目改为对应机器类型。
- 导出表包含 `NAME` 和 `DESCRIPTION`。
- 输出文件大小与 SHA-256。

建议再按项目增加：

- 禁止错误 CRT 配置。
- 检查预期扩展名 `.addon32` 或 `.addon64`。
- 检查意外依赖 DLL。
- 同时构建 x86/x64 时分别验证，不能用一个结果代替另一个。

## 5. 文档驱动的调试与测试

### Bug 记录格式

每个问题使用固定编号，例如 `WDM-002`：

```markdown
## WDM-002: Alt+Tab 后崩溃

- 首次受影响版本：1.0
- 当前诊断版本：1.1 RC14
- 环境：ReShade 6.7.3、D3D9 x86
- 症状：...
- 证据：Windows 事件日志、模块偏移、日志末尾内容
- 已否定假设：...
- 候选修复：...
- 实测结论：待确认
```

区分三类结论：

- `源码/结构验证`：编译通过、导出正确、静态检查通过。
- `诊断候选`：为了隔离变量，可能暂时牺牲功能。
- `运行时确认`：项目所有者已在目标游戏复现并验证。

禁止把“编译成功”写成“Bug 已修复”。

### 固定回归矩阵

ReShade D3D9 add-on 建议至少覆盖：

| 测试 | 检查内容 |
| --- | --- |
| 冷启动 | 主菜单、overlay、插件页面、日志 |
| 进入地图/关卡 | 分辨率 Reset、资源重建、Shader 重载 |
| 效果正确性 | 深度、AO、武器/手臂、截图一致性 |
| Alt+Tab | 第一次、第二次、连续多次 |
| 菜单切换 | Alt+Tab 前后分别打开/关闭游戏菜单 |
| 性能模式 | 开启和关闭分别测试 |
| 原生 API | D3D9/DX11 等项目目标路径 |
| 翻译层 | DXVK/Vulkan 等明确支持或安全 no-op 路径 |
| 设备 Reset | 分辨率、全屏/窗口、地图加载 |

测试结论应写入 `docs/TESTING.md` 和对应 Bug 条目。

## 6. GitHub Actions

### 持续构建

`.github/workflows/build.yml` 推荐触发条件：

```yaml
on:
  push:
    branches: [main, 'release/**']
    tags: ['v*']
  pull_request:
    branches: [main, 'release/**']
  workflow_dispatch:
```

Windows runner 使用 `windows-2022`，通过 `vswhere.exe` 找到 MSBuild，不要写死云端 Visual Studio 安装目录。

构建步骤应为：

1. Checkout。
2. 使用固定 Configuration、Platform、Toolset 和 SDK 构建。
3. 用 `dumpbin` 验证架构和导出。
4. 生成 `SHA256SUMS.txt`。
5. 用 `actions/upload-artifact@v4` 上传 DLL 和校验文件。

Artifact 名称不要直接包含 `${{ github.ref_name }}`，因为 `release/1.1` 中的 `/` 可能导致非法名称。可使用项目名加 `${{ github.run_number }}`。

### 正式发布

`.github/workflows/release.yml` 只应由 `v*` 标签或手动调试触发：

```yaml
on:
  push:
    tags: ['v*']
  workflow_dispatch:
```

正式发布流程必须重新构建和验证，不能直接拿本地历史 DLL。创建 GitHub Release 时上传：

- add-on DLL。
- `SHA256SUMS.txt`。
- 需要时增加配置示例、安装说明或符号文件。

Release job 需要：

```yaml
permissions:
  contents: write
```

## 7. 一键上传脚本

仓库根目录提供 `publish-to-github.cmd`，双击后调用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish-github.ps1
```

### PowerShell 脚本职责

一键上传脚本应按以下顺序执行，任何一步失败立即停止：

1. 检查 Git。
2. 检查 GitHub CLI `gh`；缺失时可通过 winget 安装。
3. 检查 `gh auth status`；未登录时启动网页登录。
4. 若没有 `.git`，初始化 `main`。
5. 读取并验证 `VERSION`。
6. 本地重新构建 Release。
7. 验证架构与 ReShade 导出。
8. 重新生成 SHA-256。
9. 若存在 `releases/<VERSION>/`，同步当前 DLL 和校验文件。
10. 配置当前仓库的 Git 用户名和 noreply 邮箱。
11. `git add --all`，有变化才提交。
12. 检查 `origin`；不存在时创建或连接 GitHub 仓库。
13. 推送当前分支并触发 Actions。
14. 只有显式指定正式发布参数时才创建和推送标签。

### 预发布保护

脚本在正式发布前必须检查：

```powershell
if ($version -match '-') {
    throw "VERSION '$version' is a pre-release."
}
if ($ReleaseTag -ne "v$version") {
    throw "Release tag does not match VERSION."
}
```

默认双击操作只推送候选分支，不创建正式标签。

正式版本完成运行时测试后才执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\publish-github.ps1 `
  -PublishRelease `
  -ReleaseTag v1.1
```

## 8. 打包规则

`scripts/package-release.ps1` 用临时目录组装 ZIP，排除：

- `.git/`、`.vs/`。
- `build/intermediate/`。
- `.pdb`、`.lib`、`.exp` 等非交付中间文件，除非项目决定公开符号。

ZIP 应包含：

- 完整源码与工程文件。
- README、许可证、变更记录和移交文档。
- 构建、验证、打包、发布脚本。
- GitHub Actions。
- 离线依赖和依赖许可证。
- 当前构建 DLL、SHA-256。
- 所有冻结版本目录。

输出文件名建议：

```text
ProjectName-1.1-rc3-GitHub.zip
```

压缩包默认放在项目工作区的统一归档目录，不要散落到上级目录或临时目录。打包后再次计算 ZIP 的 SHA-256。

## 9. 迁移到另一个 ReShade 项目

复制本流程时必须替换以下内容：

| WeaponDepthMerge 示例 | 新项目应替换为 |
| --- | --- |
| `WeaponDepthMerge` | GitHub 仓库名和产品名 |
| `WeaponDepthMerge.addon32` | 实际输出文件名与架构 |
| `weapon_depth_merge.vcxproj` | 实际工程文件 |
| `D3D9 x86` | 实际 API 和平台矩阵 |
| `NAME`、`DESCRIPTION` | 新插件导出文本 |
| `WDM-xxx` | 新项目 Bug 编号前缀 |
| 安装说明 | 新项目目标游戏和部署位置 |
| Actions 的 artifact 名 | 新产品名，且不得含非法字符 |
| `publish-github.ps1` 参数默认值 | 新仓库名、描述和默认标签 |

随后执行以下验收：

1. 在没有网络下载依赖的情况下完成本地构建。
2. 删除本地 `build/intermediate` 后重新构建。
3. 验证本地 DLL 架构和导出。
4. 运行打包脚本并检查 ZIP 内目录。
5. 推送候选分支，确认 GitHub Actions 变绿。
6. 下载 Actions artifact，对比架构、导出和 SHA-256。
7. 用预发布 `VERSION` 尝试正式发布，确认脚本会拒绝。
8. 只有目标游戏实测通过后才改为正式版本并打标签。

## 10. 发布前检查清单

```text
[ ] VERSION、DESCRIPTION、README、CHANGELOG 一致
[ ] _项目移交.md 已更新当前结论和下一步
[ ] BUGS.md 记录失败方案和运行时证据
[ ] TESTING.md 完成目标环境回归矩阵
[ ] Release/Win32 或目标平台构建零错误
[ ] dumpbin 验证架构和 NAME/DESCRIPTION
[ ] build/SHA256SUMS.txt 与 DLL 一致
[ ] releases/<VERSION>/ 与 build/ 当前二进制一致
[ ] third_party 依赖、来源和许可证完整
[ ] git diff --check 通过
[ ] 工作树无遗漏文件
[ ] GitHub Actions 构建成功
[ ] RC 未创建正式 v* 标签
[ ] 正式版已有目标游戏运行时确认
```

## 11. WeaponDepthMerge 中可直接参考的文件

- `scripts/build-release.ps1`：本地 Win32 Release 构建。
- `scripts/verify-binary.ps1`：PE32 和导出验证。
- `scripts/package-release.ps1`：完整 GitHub ZIP 打包。
- `scripts/publish-github.ps1`：建仓、认证、构建、校验、提交和推送。
- `publish-to-github.cmd`：双击入口。
- `.github/workflows/build.yml`：候选分支持续构建。
- `.github/workflows/release.yml`：标签触发正式 Release。

复制这些文件后必须完成第 9 节的项目名、工程名、目标架构和发布参数替换，不能原样用于另一个项目。
