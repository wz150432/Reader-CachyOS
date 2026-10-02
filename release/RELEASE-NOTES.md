# Reader CachyOS 0.1.0.r15.gd65492e.20261002

面向 CachyOS / Arch Linux x86_64 的离线 TXT / EPUB 阅读器。

## 安装或升级

下载 `.pkg.tar.zst` 附件，在下载目录执行：

```bash
sha256sum -c SHA256SUMS
sudo pacman -U ./reader-cachyos-git-0.1.0.r15.gd65492e.20261002-1-x86_64.pkg.tar.zst
```

pacman 会处理 Qt 6、libarchive 依赖。安装后在应用菜单搜索 Reader，或执行 `reader`。
安装新版使用同样的命令，无需先卸载。

## 卸载

```bash
sudo pacman -R reader-cachyos-git
```

保留个人设置、阅读记录、书签及本地书籍。

## 构建来源

本安装包从 2026-10-02 的本地工作区快照全新构建，包含基于 d65492e 的未提交修改。
附件中的源码快照是本次构建使用的源码，版本号中的 Git 提交不是完整构建来源。
上传 Release 前，应提交并推送这些修改，再在对应提交上创建版本标签。
