# Betterbox

Betterbox 是 [Bettbox](https://github.com/appshubcc/Bettbox) 的分支版本，保留上游功能，只增加下面这些能力。

## 相比 Bettbox 增加的功能

### 分流规则管理

新增独立的“分流”页面，提供类似 Surge“规则”页面的管理体验：

- 按顺序查看当前配置中的分流规则；
- 展示规则类型、匹配内容和策略目标；
- 搜索规则；
- 添加、编辑和删除规则；
- 拖拽调整规则优先级；
- 从当前配置导入原始规则；
- 保存规则覆盖并应用到当前配置；
- 复用 Bettbox 原有规则模型和配置覆盖机制，不创建第二套规则存储。

### 为当前网页设置规则（macOS）

- 浏览网页时，点击菜单栏图标 → **为当前网页设置规则…**；如果图标默认打开主窗口，可右键打开菜单，或在设置中调整托盘点击行为；
- 自动读取点击菜单前位于前台的 Safari、Chrome、Edge、Brave、Arc 或 Chromium 的当前标签页；不读取后台浏览器或浏览历史；
- 预填当前主机名的 `DOMAIN-SUFFIX` 规则（不猜测父域名），IP 地址预填 `/32` 或 `/128` 的 `IP-CIDR` 规则；可修改规则类型、匹配内容和策略；
- 确认后永久保存到当前配置的规则覆盖列表顶部，并重新应用配置；可在“分流”页面继续管理；
- 首次使用需要授权 macOS 自动化权限。应用内部名称仍为 Bettbox，可在 **系统设置 → 隐私与安全性 → 自动化** 中修改授权。

仅支持 HTTP/HTTPS 网页，规则仅在规则模式下生效。保存会启用此配置的规则覆盖，
包括已有的覆盖规则。启用脚本覆盖的配置须先关闭脚本覆盖，避免规则被脚本忽略。
暂不包含 Surge 的临时规则和“添加到规则集”功能；Windows/Linux 不显示此入口。

### 自动跟随 Bettbox 更新

`.github/workflows/sync-upstream.yml` 会定期检查 `appshubcc/Bettbox`：

- 每天检查一次，自动合并上游更新到默认分支；
- 合并成功后自动构建全部平台，并将安装包发布到 Release；
- 如果上游改动与“分流”功能冲突，则停止合并并创建 Issue，不覆盖本地代码；
- 支持在 GitHub Actions 页面手动指定上游分支。

## GitHub Actions 构建安装包

可以直接在 GitHub Actions 中构建安装包。

### 上游更新后自动构建并发布

`Sync Bettbox upstream and release Betterbox` 每天检查 Bettbox 上游。发现新提交时会：

1. 将上游更新合并到 Betterbox 默认分支；
2. 冲突时停止并创建 Issue，不构建不完整代码；
3. 合并成功后自动触发 `build` workflow；
4. 构建完成后创建 Betterbox GitHub Release 并上传安装包。

Release 标签会根据 Bettbox 版本、日期和上游提交生成。

构建矩阵包含：

- Android：ARMv7、ARM64、x86_64、Universal APK；
- Windows：x64、ARM64，以及 x64 Compatible 安装包；
- macOS：Apple Silicon、Intel，以及 Intel Compatible 安装包；
- Linux：x64、ARM64 DEB，x64 AppImage 和 RPM。

发布的安装包文件名统一以 `Betterbox-` 开头。为减少上游合并冲突，应用内部名称、
可执行文件名称和应用 ID 暂沿用 Bettbox；这不是与 Bettbox 独立共存的重命名版本。

### 手动构建

进入仓库的 **Actions → build → Run workflow**，可以选择构建全部平台或单个平台。
默认只生成 Actions Artifacts；如果设置 `release_tag` 并启用 `create_release`，则会在构建后创建 Release。

### 签名配置

构建流程沿用 Bettbox 的打包方式：

- Android 签名：配置 `KEYSTORE`、`KEY_ALIAS`、`STORE_PASSWORD`、`KEY_PASSWORD` Secrets；
- Windows 签名：配置 `SIGNPATH_API_TOKEN` 及 Bettbox SignPath 项目对应的配置；
- 未配置签名时，仍可构建用于测试的安装包，但不会获得对应平台的正式签名。

详细的上游功能、系统要求和构建依赖请参考 [Bettbox README](https://github.com/appshubcc/Bettbox#readme)。

## 相关 Action

- [同步 Bettbox 上游](.github/workflows/sync-upstream.yml)
- [构建安装包](.github/workflows/build.yaml)

## 许可证

本项目沿用 Bettbox 的 GPL-3.0 许可证。
