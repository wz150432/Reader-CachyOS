#include "core/Book.h"
#include "core/TextBook.h"
#include <QFileInfo>

namespace reader {

std::shared_ptr<Book> Book::create(const QString &filePath, QString *error)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QStringLiteral("txt")) {
        auto book = std::make_shared<TextBook>();
        if (!book->open(filePath, error))
            return nullptr;
        return book;
    }
    if (error)
        *error = QStringLiteral("暂不支持该格式（当前版本支持 TXT）");
    return nullptr;
}

}
