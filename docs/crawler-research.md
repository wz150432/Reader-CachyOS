# 在线抓取实现对照

核查日期：2026-10-19。源码研究与后续修复记录。会话缺口现已修复；其余未实施项在下文标明。

## binbyu/Reader 怎么抓小说

核查快照：`7e94be8b99461857236c287e08dff3bfecb4f86c`，当时默认分支为 `master`。

它有两条独立路径：

1. 自己抓网站：`libhttps` 发请求，`libxml2` 按 XPath 提取内容，站点配置放在 `bs.json`。
2. 连接 Legado：通过局域网接口取得手机书架、目录及正文，抓取与书源解析交给手机端。

| 环节 | 直接抓取路径的做法 | 源码 |
| --- | --- | --- |
| 搜索 | GET / POST；关键词编码单独配置为自动、UTF-8 或 GBK；自动模式先 HEAD 探测响应编码 | [OnlineDlg.cpp](https://github.com/binbyu/Reader/blob/7e94be8b99461857236c287e08dff3bfecb4f86c/Reader/OnlineDlg.cpp#L452-L594) |
| 解码 | 检测响应是否为有效 UTF-8，否则走 Windows ANSI 转换；属于依赖系统代码页的回退策略 | [OnlineDlg.cpp](https://github.com/binbyu/Reader/blob/7e94be8b99461857236c287e08dff3bfecb4f86c/Reader/OnlineDlg.cpp#L291-L317) |
| 目录 | 书籍详情页可先跳转到目录页；目录分页累计标题和 URL | [OnlineBook.cpp](https://github.com/binbyu/Reader/blob/7e94be8b99461857236c287e08dff3bfecb4f86c/Reader/OnlineBook.cpp#L1407-L1643) |
| 正文 | 正文 XPath、分页链接 XPath、翻页按钮文字 XPath 及允许文字共同控制抓取；逐页拼接 | [OnlineBook.cpp](https://github.com/binbyu/Reader/blob/7e94be8b99461857236c287e08dff3bfecb4f86c/Reader/OnlineBook.cpp#L1645-L1809) |
| 清理 | HTML 格式化后支持关键词或正则过滤 | [OnlineBook.cpp](https://github.com/binbyu/Reader/blob/7e94be8b99461857236c287e08dff3bfecb4f86c/Reader/OnlineBook.cpp#L1220-L1314) |

Legado 接口路径见 [Web 阅读说明](https://github.com/binbyu/Reader/blob/7e94be8b99461857236c287e08dff3bfecb4f86c/doc/web-read.md)：`getBookshelf`、`getChapterList`、`getBookContent`、`saveBookProgress`。这条路径不能当作 Reader 自身能够解析所有 Legado 书源的证据。

## 成熟爬虫项目值得借鉴什么

| 项目 | 已检查的实现 | 对本项目的启发 |
| --- | --- | --- |
| [Scrapy](https://github.com/scrapy/scrapy) | [Cookie 中间件](https://github.com/scrapy/scrapy/blob/master/scrapy/downloadermiddlewares/cookies.py)、[重试中间件](https://github.com/scrapy/scrapy/blob/master/scrapy/downloadermiddlewares/retry.py)、[AutoThrottle](https://github.com/scrapy/scrapy/blob/master/scrapy/extensions/throttle.py) | 会话维护、按原因统计失败、自适应请求间隔应与站点解析规则分开 |
| [Crawlee](https://github.com/apify/crawlee) | [请求队列、重试与失败处理](https://github.com/apify/crawlee/blob/master/packages/basic-crawler/src/internals/basic-crawler.ts)、[HTTP 响应解析](https://github.com/apify/crawlee/blob/master/packages/http-crawler/src/internals/http-crawler.ts)、[Playwright 路径](https://github.com/apify/crawlee/blob/master/packages/playwright-crawler/src/internals/playwright-crawler.ts) | 区分 HTTP 页面、JSON 接口与需要执行 JavaScript 的页面；保留可替换的获取层 |

本次访问时 [gedoor/legado 官方仓库](https://github.com/gedoor/legado) 的当前树只有公告 README 和图片，未取得现行解析器源码，因此不对其内部实现作未经核验的描述。

## 研究时复现的三个缺口与当前状态

使用现有 `build-online-engine/reader-online`，连接本机临时 HTTP 服务，通过真实 JSON 行协议请求；未接触正在运行的阅读器。

| 测试 | 实际结果 | 定位 |
| --- | --- | --- |
| 302 响应先设置 Cookie，跳转目标要求带回 Cookie | 返回 `HTTP 403` | `src/online/engine/NovelSourceClient.cpp:188`：Cookie 加载、保存均为 Manual，没有接收并维护服务器的新 Cookie；现已修复，按完整书源配置隔离内存 Cookie jar，同源装载/保存，新增回归测试通过 |
| 搜索页含一本书及“下一页”链接 | 找到书，但 `hasMore=false` | `src/online/engine/HtmlRuleParser.cpp:156`：HTML 搜索分页状态固定为 false，尚无搜索下一页规则 |
| GBK 搜索接口，源编码设为 auto | 零结果；显式 GB18030 的对照请求找到《红楼梦》 | `src/online/engine/NovelSourceClient.cpp:15`：auto 下关键词仍按 UTF-8 编码；响应解码发生在请求发出之后 |

这些是确定存在的能力缺口，不能据此认定某个外部网站的 403 必然由 Cookie 或编码造成。

当前已经具备的能力包括：GET / POST、自定义请求头及静态 Cookie、响应编码处理、目录与正文分页、目录 URL 去重、取消、超时、限速、部分临时错误重试和 Retry-After 处理。改进应保留这些行为。

## 上游书源是否还能用

该快照的 [bs.json](https://github.com/binbyu/Reader/blob/7e94be8b99461857236c287e08dff3bfecb4f86c/bs.json) 包含 13 个配置。本机使用应用 User-Agent 搜索《红楼梦》，抽查四个入口：

| 来源 | 本次结果 |
| --- | --- |
| 三七中文的 `12xs.top` 搜索入口 | HTTP 403 |
| 提莫書屋 | HTTP 403 |
| 追书神器吧 | 读取超时 |
| 零点看书 | TLS 证书验证失败 |

这是本机本次请求结果，不能扩大为所有地区或浏览器均不可用，也不能把“仓库里有配置”当成“现在可以用”。未关闭证书校验。

## 建议的实施顺序

1. 已完成隔离书源会话，并覆盖简单 Set-Cookie 会话引导的同路径重试。HTML 搜索分页、搜索请求编码与响应编码分离尚未实施；奇书侠按实测使用显式 GB18030。
2. 增强诊断：区分正常零结果、规则失配、验证码页、失效域名及网络错误。记录阶段、状态码、耗时和匹配数，避免记录敏感请求头及正文。
3. 参照 binbyu 的分页条件和文本过滤方式，补正文去重及清理能力；保留现有循环检测与页数上限。
4. 每个内置书源通过“搜索 → 目录 → 正文 → 正文分页 → 换章”实测再加入；定期复查。
5. 根据实际目标站再增加通用 JSON 解析或浏览器获取模块。现有独立组件边界适合承载可选依赖，无须先把完整 Scrapy/Crawlee 运行时塞进桌面程序。

后续还修复了正文分页链接位于 `<nav>` 时被正文清理提前删除的问题：先解析导航，再清理文字；回归测试覆盖跨两页正文且不带入导航或页脚。浏览器检查按正常搜索、目录、章节顺序进行，未点击整本下载。当前网络核验与本地回归记录位于忽略目录 `build-online-base/source-verification/`；站点结果见 [书源兼容性](online-sources.md)。
