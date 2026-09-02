# 发布到 AUR 并使用 paru 安装

目前仓库还没有 Git 远程、没有发布 tag，也没有 AUR 包，所以还不能直接
`paru -S reader-cachyos-git`。按下面步骤发布后即可。

## 1. 把源码推到 Git 平台

先把项目推到 GitHub / Gitea / GitLab 等平台，例如：

```bash
git remote add origin https://github.com/YOUR_USERNAME/Reader-CachyOS.git
git push -u origin main
```

可选：给稳定版本打 tag：

```bash
git tag v0.1.0
git push origin v0.1.0
```

## 2. 创建 AUR 账号

1. 打开 https://aur.archlinux.org 注册账号。
2. 在账号设置里添加 SSH 公钥：
   `ssh-keygen -t ed25519 -C "your@email"`，然后把
   `~/.ssh/id_ed25519.pub` 内容粘贴进去。

## 3. 准备并测试 PKGBUILD

先编辑 `packaging/PKGBUILD`：

- 把 `YOUR_USERNAME` 换成真实 Git 用户名；
- 把 `# Maintainer: YOUR NAME <your@email.example>` 换成自己的名字和邮箱；
- 如果仓库不是 GitHub，修改 `url` 和 source 里的地址。

本目录里的 PKGBUILD 是 `*-git` 模板，直接跟踪 Git 主分支，不需要维护
sha256sums。想发布稳定版时再改成上传 release tarball 的方式。

本地先检查语法和打包：

```bash
cd packaging
makepkg -f
namcap PKGBUILD reader-cachyos-git-*.pkg.tar.zst
```

## 4. 创建 AUR 仓库并推送

在 https://aur.archlinux.org/packages 页面的“Create”里创建包，名称必须和
`pkgname` 相同（建议用 `reader-cachyos-git`，不要用过于通用的 `reader`）。

```bash
git clone ssh://aur@aur.archlinux.org/reader-cachyos-git.git
cd reader-cachyos-git
cp ../Reader-CachyOS/packaging/PKGBUILD .
cp ../Reader-CachyOS/packaging/reader.install .
makepkg --printsrcinfo > .SRCINFO

git add PKGBUILD .SRCINFO reader.install
git commit -m "Initial package"
git push origin master
```

推送后 AUR 页面会自动显示包信息。

## 5. 别人怎么安装

已安装 paru / yay 的 Arch/CachyOS 用户：

```bash
paru -S reader-cachyos-git
# 或者搜索
paru -Ss reader-cachyos
```

包会从 Git 拉源码编译，安装后：

- 命令行启动：`reader`
- 应用菜单/搜索栏搜索：`Reader` 或 `阅读器`
- TXT/EPUB/MOBI 文件默认使用 Reader 打开

## 6. 以后更新

`*-git` 包每次重新安装会自动拉最新提交。源码发布新版本后：

```bash
paru -Syu
```

如果改成稳定版 AUR 包，需要用 release tarball 地址替换 source，并用
`makepkg -g` 生成新的 sha256sums，同时保持 tag 与 `pkgver` 一致。
