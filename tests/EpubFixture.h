#pragma once
#include <QTemporaryDir>
#include <QFile>
#include <archive.h>
#include <archive_entry.h>

static QString fixture(const QTemporaryDir &dir, const QByteArray &spine = "<itemref idref='b'/><itemref idref='a'/>", bool ncx = false, bool navigation = true)
{
    const QString path = dir.filePath("中文.EPUB");
    QVector<QPair<QByteArray, QByteArray>> entries = {
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", "<container xmlns='urn:oasis:names:tc:opendocument:xmlns:container'><rootfiles><rootfile full-path='OPS/package.opf'/></rootfiles></container>"},
        {"OPS/package.opf", "<package xmlns='http://www.idpf.org/2007/opf'><metadata><dc:title xmlns:dc='http://purl.org/dc/elements/1.1/'>测试书</dc:title></metadata><manifest><item id='cover' href='cover.svg' media-type='image/svg+xml'/><item id='a' href='text/a.xhtml' media-type='application/xhtml+xml'/><item id='b' href='text/%E4%B9%99.xhtml' media-type='application/xhtml+xml'/><item id='nav' href='nav.xhtml' properties='nav' media-type='application/xhtml+xml'/><item id='ncx' href='toc.ncx' media-type='application/x-dtbncx+xml'/></manifest><spine toc='ncx'>" + spine + "</spine></package>"},
        {"OPS/cover.svg", "<svg xmlns='http://www.w3.org/2000/svg'><rect width='100' height='100'/></svg>"},
        {"OPS/text/a.xhtml", "<html xmlns='http://www.w3.org/1999/xhtml'><head><title>隐藏标题</title><style>隐藏样式</style></head><body><h1>甲章</h1><p>Alpha &amp; beta.</p><p>第二段<br/>下一行</p><script>隐藏脚本</script></body></html>"},
        {"OPS/text/乙.xhtml", "<html xmlns='http://www.w3.org/1999/xhtml'><body><h1>乙章</h1><p>中文正文</p></body></html>"}
    };
    if (navigation && ncx)
        entries.append(QPair<QByteArray, QByteArray>{"OPS/toc.ncx", "<ncx><navMap><navPoint><navLabel><text>目录乙</text></navLabel><content src='text/%E4%B9%99.xhtml#start'/></navPoint><navPoint><navLabel><text>目录甲</text></navLabel><content src='text/a.xhtml'/></navPoint></navMap></ncx>"});
    else if (navigation)
        entries.append(QPair<QByteArray, QByteArray>{"OPS/nav.xhtml", "<html xmlns:epub='http://www.idpf.org/2007/ops'><body><nav epub:type='landmarks'><a href='text/a.xhtml'>错误标签</a></nav><nav epub:type='toc'><ol><li><a href='text/%E4%B9%99.xhtml#start'>目录乙</a></li><li><a href='text/a.xhtml'>目录甲</a></li></ol></nav></body></html>"});
    archive *writer = archive_write_new();
    archive_write_set_format_zip(writer);
    archive_write_open_filename(writer, QFile::encodeName(path).constData());
    for (const auto &entry : entries) {
        archive_entry *e = archive_entry_new();
        archive_entry_set_pathname_utf8(e, entry.first.constData());
        archive_entry_set_filetype(e, AE_IFREG);
        archive_entry_set_perm(e, 0644);
        archive_entry_set_size(e, entry.second.size());
        archive_write_header(writer, e);
        archive_write_data(writer, entry.second.constData(), entry.second.size());
        archive_entry_free(e);
    }
    archive_write_close(writer);
    archive_write_free(writer);
    return path;
}
