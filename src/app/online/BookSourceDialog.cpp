#include "BookSourceDialog.h"
#include "app/UiStyle.h"
#include "core/online/OnlineEngineBridge.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QUuid>
#include <QVBoxLayout>
namespace reader {
BookSourceDialog::BookSourceDialog(BookSourceStore *s, OnlineEngineBridge *b,
                                   QWidget *p)
    : QDialog(p), m_store(s), m_bridge(b),
      m_session(QUuid::createUuid().toString()) {
  setWindowTitle(tr("管理书源"));
  setObjectName("bookSourceDialog");
  setStyleSheet(readerUiStyle());
  resize(860, 780);
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);
  auto top = new QHBoxLayout;
  layout->addLayout(top);
  m_sources = new QComboBox(this);
  m_sources->setObjectName("sourceList");
  top->addWidget(m_sources, 1);
  auto add = new QPushButton(tr("新建"), this);
  auto copy = new QPushButton(tr("复制"), this);
  copy->setObjectName("copySource");
  auto remove = new QPushButton(tr("删除"), this);
  top->addWidget(add);
  top->addWidget(copy);
  top->addWidget(remove);
  auto tabs = new QTabWidget(this);
  layout->addWidget(tabs, 1);
  auto page = [&](const QString &name) {
    auto area = new QScrollArea(this);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    auto body = new QWidget;
    auto f = new QFormLayout(body);
    f->setContentsMargins(12, 12, 12, 12);
    f->setSpacing(10);
    f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    area->setWidget(body);
    tabs->addTab(area, name);
    return f;
  };
  auto basic = page(tr("基本信息"));
  auto search = page(tr("搜索规则"));
  auto rules = page(tr("目录与正文"));
  auto advanced = page(tr("请求设置"));
  auto field = [&](QFormLayout *f, const char *key, const QString &label,
                   const QString &example = QString()) {
    auto edit = new QLineEdit(this);
    edit->setObjectName(key);
    edit->setPlaceholderText(example);
    edit->setClearButtonEnabled(true);
    m_fields[key] = edit;
    f->addRow(label, edit);
  };
  field(basic, "id", tr("唯一标识"), "my-source");
  field(basic, "name", tr("书源名称"));
  m_kind = new QComboBox(this);
  m_kind->addItems({"html", "mediawiki"});
  basic->addRow(tr("网站类型"), m_kind);
  field(basic, "baseUrl", tr("网站地址"), "https://example.org");
  m_enabled = new QCheckBox(tr("启用此书源"), this);
  basic->addRow(m_enabled);
  auto hint = new QLabel(tr("搜索可以留空，保存后使用书籍网址导入。提取规则使用"
                            " XPath；搜索标题、链接和作者相对于单条结果。"),
                         this);
  hint->setWordWrap(true);
  search->addRow(hint);
  field(search, "searchUrl", tr("搜索网址"),
        "https://example.org/search?q={keyword}&page={page}");
  m_method = new QComboBox(this);
  m_method->addItems({"GET", "POST"});
  search->addRow(tr("请求方式"), m_method);
  field(search, "searchBody", tr("POST 表单"), "q={keyword}&page={page}");
  field(search, "resultXPath", tr("搜索结果位置"), "//div[@class='book']");
  field(search, "titleXPath", tr("标题位置"), ".//a");
  field(search, "linkXPath", tr("链接位置"), ".//a/@href");
  field(search, "authorXPath", tr("作者位置（可选）"),
        ".//span[@class='author']");
  field(rules, "tocUrlXPath", tr("目录页链接（可选）"), "//a[@id='toc']/@href");
  field(rules, "chapterXPath", tr("目录链接位置"), "//div[@id='list']//a");
  field(rules, "contentXPath", tr("正文位置"), "//div[@id='content']");
  field(rules, "removeXPath", tr("移除内容（可选）"), "//script | //style");
  field(rules, "tocNextXPath", tr("目录下一页（可选）"),
        "//a[@rel='next']/@href");
  field(rules, "contentNextXPath", tr("正文下一页（可选）"),
        "//a[@rel='next']/@href");
  m_reverse = new QCheckBox(tr("目录倒序"), this);
  rules->addRow(m_reverse);
  auto limits = new QLabel(tr("未配置分页时只提取当前页。目录最多 100 页 / "
                              "20,000 章；单章最多 20 页。"),
                           this);
  limits->setWordWrap(true);
  rules->addRow(limits);
  m_encoding = new QComboBox(this);
  m_encoding->addItems({"auto", "UTF-8", "GB18030"});
  advanced->addRow(tr("网页编码"), m_encoding);
  m_interval = new QSpinBox(this);
  m_interval->setRange(500, 3600000);
  m_interval->setValue(1000);
  m_interval->setSuffix(" ms");
  advanced->addRow(tr("请求间隔"), m_interval);
  field(advanced, "cookie", tr("Cookie（仅本机保存）"));
  m_fields["cookie"]->setEchoMode(QLineEdit::Password);
  m_headers = new QPlainTextEdit(this);
  m_headers->setObjectName("headers");
  m_headers->setPlaceholderText("{\"User-Agent\": \"Reader\"}");
  m_headers->setMaximumHeight(100);
  advanced->addRow(tr("自定义请求头（JSON）"), m_headers);
  auto protect = new QCheckBox(tr("显示请求头"), this);
  advanced->addRow(protect);
  m_headers->setVisible(false);
  connect(protect, &QCheckBox::toggled, m_headers, &QWidget::setVisible);
  auto testPage = new QWidget(this);
  auto testLayout = new QVBoxLayout(testPage);
  tabs->addTab(testPage, tr("分步测试"));
  auto testHint =
      new QLabel(tr("输入关键词测试搜索，或输入书籍网址测试目录；选中目录中的一"
                    "章后再提取正文。不会预取其他章节。"),
                 this);
  testHint->setWordWrap(true);
  testLayout->addWidget(testHint);
  m_testInput = new QLineEdit(this);
  m_testInput->setObjectName("testInput");
  m_testInput->setPlaceholderText(tr("关键词或 https://… 书籍网址"));
  testLayout->addWidget(m_testInput);
  auto buttons = new QHBoxLayout;
  testLayout->addLayout(buttons);
  for (auto pair :
       {qMakePair("search", tr("测试搜索")), qMakePair("toc", tr("测试目录")),
        qMakePair("chapter", tr("测试选中章节"))}) {
    auto button = new QPushButton(pair.second, this);
    button->setObjectName(pair.first == QString("search") ? "testSearch"
                          : pair.first == QString("toc")  ? "testToc"
                                                          : "testChapter");
    buttons->addWidget(button);
    connect(button, &QPushButton::clicked, this,
            [this, op = QString(pair.first)] { test(op); });
    if (pair.first == QString("chapter"))
      m_testChapter = button;
  }
  auto cancel = new QPushButton(tr("取消测试"), this);
  buttons->addWidget(cancel);
  connect(cancel, &QPushButton::clicked, this, [this] {
    stopTest();
    m_error->setText(tr("测试已取消"));
  });
  m_testItems = new QListWidget(this);
  m_testItems->setObjectName("testItems");
  testLayout->addWidget(m_testItems, 1);
  m_preview = new QPlainTextEdit(this);
  m_preview->setReadOnly(true);
  m_preview->setObjectName("testPreview");
  testLayout->addWidget(m_preview, 1);
  connect(m_testItems, &QListWidget::itemActivated, this,
          [this](QListWidgetItem *) {
            if (m_operation == "search" || m_choices) {
              int i = m_testItems->currentRow();
              if (i >= 0 && i < m_testRows.size()) {
                m_testInput->setText(
                    m_testRows[i].toObject()["url"].toString());
                test("toc");
              }
            }
          });
  m_error = new QLabel(this);
  m_error->setObjectName("sourceError");
  m_error->setWordWrap(true);
  layout->addWidget(m_error);
  auto bottom = new QHBoxLayout;
  layout->addLayout(bottom);
  auto save = new QPushButton(tr("保存书源"), this);
  save->setDefault(true);
  save->setObjectName("saveSource");
  bottom->addWidget(save);
  bottom->addStretch();
  auto close = new QPushButton(tr("关闭"), this);
  bottom->addWidget(close);
  connect(close, &QPushButton::clicked, this, &QDialog::close);
  connect(save, &QPushButton::clicked, this, &BookSourceDialog::saveSource);
  for (auto &source : s->sources())
    m_sources->addItem(source.toJson()["name"].toString(), source.id());
  connect(m_sources, &QComboBox::currentIndexChanged, this,
          [this] { loadSource(); });
  connect(copy, &QPushButton::clicked, this, [this] {
    m_readOnly = false;
    for (auto *edit : m_fields)
      edit->setReadOnly(false);
    m_fields["id"]->setText("source-" +
                            QUuid::createUuid().toString(QUuid::Id128));
    m_fields["name"]->setText(m_fields["name"]->text() + tr(" 副本"));
    m_error->setText(tr("副本尚未保存，可修改规则后保存。"));
  });
  connect(add, &QPushButton::clicked, this, [this] {
    stopTest();
    m_readOnly = false;
    for (auto *edit : m_fields) {
      edit->setReadOnly(false);
      edit->clear();
    }
    m_fields["id"]->setText("source-" +
                            QUuid::createUuid().toString(QUuid::Id128));
    m_kind->setCurrentText("html");
    m_method->setCurrentText("GET");
    m_encoding->setCurrentText("auto");
    m_interval->setValue(1000);
    m_enabled->setChecked(true);
    m_reverse->setChecked(false);
    m_headers->setPlainText("{}");
    m_testInput->clear();
    m_testItems->clear();
    m_preview->clear();
    m_testRows = {};
    m_saveCandidate = {};
    m_operation.clear();
    m_choices = false;
    m_error->setText(tr("新书源尚未保存，请填写网站地址和提取规则。"));
  });
  connect(remove, &QPushButton::clicked, this, [this] {
    QString e;
    if (!m_store->remove(m_sources->currentData().toString(), &e) ||
        !m_store->save(&e)) {
      m_error->setText(e);
      return;
    }
    m_sources->removeItem(m_sources->currentIndex());
    emit sourcesChanged();
  });
  connect(
      b, &OnlineEngineBridge::result, this,
      [this](const QString &id, const QString &session, const QJsonObject &r) {
        if (id != m_request || session != m_session)
          return;
        m_request.clear();
        showResult(r);
      });
  connect(b, &OnlineEngineBridge::error, this,
          [this](const QString &id, const QString &session, const QString &e) {
            if (id != m_request || session != m_session)
              return;
            m_request.clear();
            m_error->setText(
                tr("测试失败：%1。检查当前步骤规则后重试。").arg(e));
          });
  loadSource();
}
BookSourceDialog::~BookSourceDialog() { stopTest(); }
void BookSourceDialog::loadSource() {
  stopTest();
  int i = m_sources->currentIndex();
  if (i < 0 || i >= m_store->sources().size())
    return;
  auto j = m_store->sources()[i].toJson();
  m_readOnly = j["id"] == BookSource::preset().id();
  for (auto it = m_fields.begin(); it != m_fields.end(); ++it) {
    it.value()->setText(j[it.key()].toString());
    it.value()->setReadOnly(m_readOnly);
  }
  m_kind->setCurrentText(j["kind"].toString());
  m_method->setCurrentText(j["searchMethod"].toString("GET"));
  m_encoding->setCurrentText(j["encoding"].toString("auto"));
  m_interval->setValue(j["intervalMs"].toInt(1000));
  m_enabled->setChecked(j["enabled"].toBool(true));
  m_reverse->setChecked(j["reverseToc"].toBool());
  m_headers->setPlainText(QString::fromUtf8(
      QJsonDocument(j["headers"].toObject()).toJson(QJsonDocument::Compact)));
  m_error->setText(m_readOnly ? tr("内置书源只读；点击“复制”创建可编辑书源。")
                              : QString());
}
BookSource BookSourceDialog::formSource(QString *error) const {
  QJsonObject j;
  for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
    j[it.key()] = it.value()->text();
  j["kind"] = m_kind->currentText();
  j["searchMethod"] = m_method->currentText();
  j["encoding"] = m_encoding->currentText();
  j["intervalMs"] = m_interval->value();
  j["enabled"] = m_enabled->isChecked();
  j["reverseToc"] = m_reverse->isChecked();
  QJsonParseError parse;
  auto headers =
      QJsonDocument::fromJson(m_headers->toPlainText().toUtf8(), &parse);
  if (parse.error != QJsonParseError::NoError || !headers.isObject()) {
    if (error)
      *error = tr("请求头必须是 JSON 对象");
    return BookSource();
  }
  j["headers"] = headers.object();
  return BookSource(j);
}
void BookSourceDialog::saveSource() {
  if (m_readOnly) {
    m_error->setText(tr("内置书源不可修改，请先复制。"));
    return;
  }
  QString e;
  auto source = formSource(&e);
  if (!e.isEmpty() || !source.validate(&e)) {
    m_error->setText(tr("无法保存：%1").arg(e));
    return;
  }
  stopTest();
  m_saveCandidate = source.toJson();
  m_operation = "save";
  m_request =
      m_bridge->request("validate", {{"source", m_saveCandidate}}, m_session);
  m_error->setText(tr("正在校验 XPath 规则…"));
}
void BookSourceDialog::commitSource() {
  QString e;
  BookSource source(m_saveCandidate);
  if (!m_store->upsert(source, &e) || !m_store->save(&e)) {
    m_error->setText(e);
    return;
  }
  int i = m_sources->findData(source.id());
  if (i < 0) {
    m_sources->addItem(source.toJson()["name"].toString(), source.id());
    i = m_sources->count() - 1;
  } else
    m_sources->setItemText(i, source.toJson()["name"].toString());
  m_sources->setCurrentIndex(i);
  m_error->setText(tr("书源已保存，凭据仅保存在本机。"));
  emit sourcesChanged();
}
void BookSourceDialog::stopTest() {
  if (!m_request.isEmpty()) {
    auto id = m_request;
    m_request.clear();
    m_bridge->cancel(id);
  }
}
void BookSourceDialog::test(const QString &op) {
  stopTest();
  QString e;
  auto source = formSource(&e);
  if (!e.isEmpty() || !source.validate(&e)) {
    m_error->setText(e);
    return;
  }
  QJsonObject fields{{"source", source.toJson()}};
  if (op == "search") {
    if (source.toJson()["kind"] == "html" &&
        source.toJson()["searchUrl"].toString().isEmpty()) {
      m_error->setText(tr("此书源仅支持网址导入，请测试目录。"));
      return;
    }
    fields["keyword"] = m_testInput->text();
    fields["page"] = 1;
  } else if (op == "toc") {
    int i = m_testItems->currentRow();
    if ((m_operation == "search" || m_choices) && i >= 0 &&
        i < m_testRows.size())
      fields["url"] = m_testRows[i].toObject()["url"];
    else
      fields["url"] = m_testInput->text();
  } else {
    int i = m_testItems->currentRow();
    if (m_choices || m_operation != "toc" || i < 0 || i >= m_testRows.size()) {
      m_error->setText(tr("请先测试目录并选择一章。"));
      return;
    }
    fields["url"] = m_testRows[i].toObject()["url"];
  }
  m_operation = op;
  m_request = m_bridge->request(op, fields, m_session);
  m_error->setText(tr("正在测试%1…可随时取消").arg(op));
}
void BookSourceDialog::showResult(const QJsonObject &r) {
  if (m_operation == "save") {
    commitSource();
    return;
  }
  m_error->setText(tr("测试完成"));
  if (m_operation == "chapter") {
    m_preview->setPlainText(r["title"].toString() + "\n" + r["url"].toString() +
                            "\n\n" + r["text"].toString());
    m_operation = "toc";
    return;
  }
  m_choices = !r["choices"].toArray().isEmpty();
  m_testRows = m_operation == "search" ? r["books"].toArray()
                                       : (m_choices ? r["choices"].toArray()
                                                    : r["chapters"].toArray());
  m_testItems->clear();
  for (auto row : m_testRows) {
    auto o = row.toObject();
    auto item = new QListWidgetItem(o["title"].toString(), m_testItems);
    item->setToolTip(o["url"].toString());
  }
  if (m_testRows.isEmpty())
    m_error->setText(tr("没有提取到结果，请检查当前步骤的规则。"));
  else
    m_testItems->setCurrentRow(0);
  if (m_choices)
    m_error->setText(tr("请选择版本，双击后测试该版本目录。"));
}
} // namespace reader
