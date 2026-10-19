#include "HtmlRuleParser.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <memory>
namespace {
QString value(xmlNodePtr n) {
  auto v = xmlNodeGetContent(n);
  QString s = QString::fromUtf8(reinterpret_cast<const char *>(v));
  xmlFree(v);
  return s.trimmed();
}
QString plain(xmlNodePtr n) {
  QString s;
  for (auto c = n; c; c = c->next) {
    if (c->type == XML_TEXT_NODE)
      s += QString::fromUtf8(reinterpret_cast<const char *>(c->content));
    else if (c->type == XML_ELEMENT_NODE) {
      QString tag =
          QString::fromLatin1(reinterpret_cast<const char *>(c->name));
      if (tag == "script" || tag == "style" || tag == "nav")
        continue;
      if (tag == "br")
        s += '\n';
      else {
        bool block =
            QStringList{"p", "div", "section", "h1", "h2", "h3", "li", "table"}
                .contains(tag);
        if (block)
          s += "\n\n";
        s += plain(c->children);
        if (block)
          s += "\n\n";
      }
    }
  }
  return s;
}
} // namespace
ParsedPage HtmlRuleParser::parse(const QByteArray &body, const QUrl &url,
                                 const QJsonObject &source, const QString &op) {
  ParsedPage out;
  QString enc = source["encoding"].toString();
  const char *encoding = nullptr;
  QByteArray e;
  if (enc != "auto" && !enc.isEmpty()) {
    e = enc.toLatin1();
    encoding = e.constData();
  } else if (!body.contains("charset=") && !body.contains("encoding=")) {
    encoding = "UTF-8";
  }
  auto doc = htmlReadMemory(body.constData(), body.size(), nullptr, encoding,
                            HTML_PARSE_NONET | HTML_PARSE_NOERROR |
                                HTML_PARSE_NOWARNING | HTML_PARSE_RECOVER);
  if (!doc) {
    out.error = "Invalid HTML";
    return out;
  }
  auto ctx = xmlXPathNewContext(doc);
  auto nodes = [&](QString xp, xmlNodePtr root = nullptr) {
    QList<xmlNodePtr> list;
    if (xp.isEmpty())
      return list;
    ctx->node = root ? root : xmlDocGetRootElement(doc);
    auto compiled = xmlXPathCompile(
        reinterpret_cast<const xmlChar *>(xp.toUtf8().constData()));
    if (!compiled) {
      out.error = "Invalid XPath";
      return list;
    }
    auto r = xmlXPathCompiledEval(compiled, ctx);
    xmlXPathFreeCompExpr(compiled);
    if (!r) {
      out.error = "Invalid XPath";
      return list;
    }
    if (r->type == XPATH_NODESET && r->nodesetval)
      for (int i = 0; i < r->nodesetval->nodeNr; i++)
        list << r->nodesetval->nodeTab[i];
    xmlXPathFreeObject(r);
    return list;
  };
  auto first = [&](QString xp, xmlNodePtr n = nullptr, bool link = false) {
    if (xp.isEmpty())
      return QString();
    ctx->node = n ? n : xmlDocGetRootElement(doc);
    auto r = xmlXPathEvalExpression(
        reinterpret_cast<const xmlChar *>(xp.toUtf8().constData()), ctx);
    if (!r) {
      out.error = "Invalid XPath";
      return QString();
    }
    QString text;
    if (r->type == XPATH_NODESET) {
      if (r->nodesetval && r->nodesetval->nodeNr) {
        auto node = r->nodesetval->nodeTab[0];
        if (link && node->type == XML_ELEMENT_NODE) {
          auto href = xmlGetProp(node, BAD_CAST "href");
          text = QString::fromUtf8(reinterpret_cast<const char *>(href)).trimmed();
          xmlFree(href);
        } else
          text = value(node);
      }
    } else {
      auto scalar = xmlXPathCastToString(r);
      text = QString::fromUtf8(reinterpret_cast<const char *>(scalar)).trimmed();
      xmlFree(scalar);
    }
    xmlXPathFreeObject(r);
    return text;
  };
  auto absolute = [&](QString v) {
    if (v.trimmed().isEmpty())
      return QString();
    auto u = url.resolved(QUrl(v));
    return (u.scheme() == "https" || u.scheme() == "http") &&
                   u.userInfo().isEmpty()
               ? u.toString()
               : QString();
  };
  QString title = first("//title");
  QString lower = QString::fromUtf8(body).toLower();
  if (lower.contains("cf-chl-") || lower.contains("just a moment") ||
      lower.contains("enable javascript and cookies") ||
      lower.contains("checking your browser"))
    out.error = "Browser challenge";
  // Pagination controls often live inside <nav>, which is excluded from
  // readable text. Resolve navigation before cleaning the content tree.
  const QString nextPage = first(source[op == "toc" ? "tocNextXPath" : "contentNextXPath"].toString(), nullptr, true);
  const QString contentsPage = op == "toc" ? first(source["tocUrlXPath"].toString(), nullptr, true) : QString();
  auto removed = nodes("//script|//style|//nav");
  removed += nodes(source["removeXPath"]
                       .toString()); // unlink only: overlapping selections
                                     // remain valid until document destruction
  QList<xmlNodePtr> detached;
  for (auto n : removed) {
    bool covered = detached.contains(n);
    for (auto p = n->parent; p; p = p->parent)
      if (removed.contains(p))
        covered = true;
    if (!covered) {
      xmlUnlinkNode(n);
      detached << n;
    }
  }
  out.result = {{"title", title}, {"url", url.toString()}};
  if (op == "search") {
    QJsonArray books;
    for (auto n : nodes(source["resultXPath"].toString())) {
      QString link = first(source["linkXPath"].toString(), n, true);
      QString t = first(source["titleXPath"].toString(), n);
      auto u = absolute(link);
      if (!u.isEmpty() && !t.isEmpty())
        books.append(QJsonObject{
            {"title", t},
            {"url", u},
            {"author", first(source["authorXPath"].toString(), n)}});
    }
    out.result = {{"books", books}, {"hasMore", false}};
  } else if (op == "toc") {
    QJsonArray chapters;
    for (auto n : nodes(source["chapterXPath"].toString())) {
      QString link;
      if (n->type == XML_ATTRIBUTE_NODE)
        link = value(n);
      else {
        auto a = xmlGetProp(n, BAD_CAST "href");
        link = QString::fromUtf8(reinterpret_cast<const char *>(a));
        xmlFree(a);
      }
      QString u = absolute(link);
      if (!u.isEmpty() && !link.isEmpty())
        chapters.append(QJsonObject{{"title", value(n)}, {"url", u}});
    }
    out.result["chapters"] = chapters;
    out.result["choices"] = QJsonArray();
    out.next = nextPage;
    if (chapters.isEmpty() && out.next.isEmpty())
      out.next = contentsPage;
  } else {
    QString text;
    for (auto n : nodes(source["contentXPath"].toString()))
      text += plain(n->children) + "\n\n";
    text.replace(QRegularExpression("[ \\t]+\\n"), "\n");
    text.replace(QRegularExpression("\\n{3,}"), "\n\n");
    text = text.trimmed();
    if (text.isEmpty() && out.error.isEmpty())
      out.error = "Empty content";
    out.result["text"] = text;
    out.next = nextPage;
  }
  if (!out.next.isEmpty())
    out.next = absolute(out.next);
  xmlXPathFreeContext(ctx);
  for (auto n : detached)
    xmlFreeNode(n);
  xmlFreeDoc(doc);
  return out;
}

QString HtmlRuleParser::validateRules(const QJsonObject &source) {
  // Compile in a private context so diagnostics never print expressions or secrets.
  auto ctx=xmlXPathNewContext(nullptr);
  ctx->error=[](void *, const xmlError *) {};
  QString error;
  for (const auto &key : {"resultXPath", "titleXPath", "linkXPath", "authorXPath", "tocUrlXPath", "chapterXPath", "contentXPath", "removeXPath", "tocNextXPath", "contentNextXPath"}) {
    auto value=source.value(key).toString();
    if(value.isEmpty())continue;
    auto compiled=xmlXPathCtxtCompile(ctx,reinterpret_cast<const xmlChar*>(value.toUtf8().constData()));
    if(!compiled){error=QString("Invalid XPath: %1").arg(key);break;}
    xmlXPathFreeCompExpr(compiled);
  }
  xmlXPathFreeContext(ctx);
  return error;
}
