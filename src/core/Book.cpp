#include "core/Book.h"
#include "core/TextBook.h"
#include "core/EpubBook.h"
#include "core/StructuredBook.h"
#include <QFileInfo>
#include "core/online/OnlineBook.h"

namespace reader {

std::shared_ptr<Book> Book::create(
    const QString &filePath, QString *error,
    const QRegularExpression &chapterRegex)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "rnovel") {
        auto book = std::make_shared<OnlineBook>();
        if (!book->open(filePath, error)) return nullptr;
        return book;
    }
    if (ext == QStringLiteral("txt")) {
        auto book = std::make_shared<TextBook>();
        if (!book->open(filePath, error, chapterRegex))
            return nullptr;
        return book;
    }
    if (ext == QStringLiteral("epub")) {
        auto book = std::make_shared<EpubBook>();
        if (!book->open(filePath, error, chapterRegex))
            return nullptr;
        return book;
    }
    if (ext == "mobi" || ext == "azw" || ext == "azw3" || ext == "fb2") {
        auto book = std::make_shared<StructuredBook>();
        if (!book->open(filePath, error, chapterRegex)) return nullptr;
        return book;
    }
    if (error)
        *error = QStringLiteral("暂不支持该格式（当前版本支持 TXT / EPUB / MOBI / AZW3 / FB2）");
    return nullptr;
}

}
