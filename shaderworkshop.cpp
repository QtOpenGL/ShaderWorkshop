#include "shaderworkshop.h"
#include "renderer.h"
#include "ui_shaderworkshop.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>

ShaderWorkshop::ShaderWorkshop(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ShaderWorkshop),
    imagePage(Q_NULLPTR),
    defaultItemName("Add buffer"),
    maxBufferPages(5),
    imagePageIndex(0),
    imageEffectCreated(false)
{
    ui->setupUi(this);

    renderer = ui->openGLWidget;

    setupWidgets();
    createMenus();
}

ShaderWorkshop::~ShaderWorkshop()
{
    qDeleteAll(pages);
    delete ui;
}

void ShaderWorkshop::paintEvent(QPaintEvent *event)
{
    if (!imageEffectCreated) {
        // create effect for main image page right before painting.
        // By doing it here we can be sure that Renderer already initialized
        // its OpenGL context
        renderer->createEffect(imagePageIndex);

        imageEffectCreated = true;
    }

    QWidget::paintEvent(event);
}

void ShaderWorkshop::newBufferRequested(const QString &name)
{
    // default combo box item activations should not create new buffers
    if (name == defaultItemName) {
        return;
    }

    Q_ASSERT(pages.contains(name));

    EditorPage *page = pages.value(name);
    page->setShaderSource(renderer->defaultFragmentShader());
    page->clearShaderLog();

    tab->insertTab(tab->count(), page, name);
    comboBox->removeItem(comboBox->findText(name));
    // always show default combo box item
    comboBox->setCurrentText(defaultItemName);

    renderer->createEffect(pageIndex(page));
}

void ShaderWorkshop::bufferCloseRequested(int tabIndex)
{
    QString name = tab->tabText(tabIndex);

    Q_ASSERT(pages.contains(name));

    tab->removeTab(tabIndex);
    comboBox->addItem(name);

    EditorPage *page = pages.value(name);

    renderer->deleteEffect(pageIndex(page));
}

void ShaderWorkshop::setupWidgets()
{
    tab = ui->tabWidget;
    tab->setTabsClosable(true);

    Q_ASSERT(tab->count() == 0);

    comboBox = ui->comboBox;
    comboBox->addItem(defaultItemName);

    PagesData pagesData;
    int pageIndex = imagePageIndex + 1;

    for (int i = 1; i < maxBufferPages; i++, pageIndex++) {
        pagesData.append(QPair<int, QString>(pageIndex, bufferName(i - 1)));
    }

    createImagePage(pagesData);

    createPages(pagesData);

    connect(comboBox, static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::activated),
            this, &ShaderWorkshop::newBufferRequested);

    connect(tab, SIGNAL(tabCloseRequested(int)),
            this, SLOT(bufferCloseRequested(int)));
}

EditorPage* ShaderWorkshop::createPage(const QString &name, int pageIndex,
                                       const PagesData &data)
{
    EditorPage *page = new EditorPage(pageIndex, data);

    pages[name] = page;
    pageIndices[page] = pageIndex;

    return page;
}

void ShaderWorkshop::createImagePage(const PagesData &data)
{
    imagePage = createPage("Image", imagePageIndex, data);
    imagePage->setShaderSource(renderer->defaultFragmentShader());

    int tabIndex = tab->insertTab(tab->count(), imagePage, tr("Image"));
    // user should not be able to close main image page
    tab->tabBar()->setTabButton(tabIndex, QTabBar::LeftSide, Q_NULLPTR);
}

void ShaderWorkshop::createPages(const PagesData &data)
{
    for (const auto &item : data) {
        const QString &name = item.second;

        createPage(name, item.first, data);
        comboBox->addItem(name);
    }
}

void ShaderWorkshop::createMenus()
{
    QMenuBar *bar = new QMenuBar(this);
    QMenu *file = bar->addMenu(tr("&File"));
    QMenu *build = bar->addMenu(tr("&Build"));

    file->addAction(ui->actionOpen);
    file->addAction(ui->actionSave);
    build->addAction(ui->actionRecompile_Shader);
}

QString ShaderWorkshop::bufferName(int index) const
{
    char letter = 'A' + index;

    return QString("Buffer %1").arg(letter);
}

EditorPage* ShaderWorkshop::currentPage() const
{
    return static_cast<EditorPage*>(tab->currentWidget());
}

int ShaderWorkshop::pageIndex(EditorPage *page) const
{
    Q_ASSERT(pageIndices.contains(page));

    return pageIndices.value(page);
}

void ShaderWorkshop::on_actionRecompile_Shader_triggered()
{
    EditorPage *page = currentPage();
    const QString source = page->shaderSource();

    QString log = renderer->recompileEffectShader(pageIndex(page), source);

    page->shaderLogUpdated(log);
}

void ShaderWorkshop::on_actionOpen_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Shader"), "",
                        tr("GLSL Fragment shader (*.frag);; Text file (*.txt)"));

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not open file %1:\n%2")
                                .arg(fileName)
                                .arg(file.errorString()));
        return;
    }

    currentPage()->setShaderSource(file.readAll());
}

void ShaderWorkshop::on_actionSave_triggered()
{
    const EditorPage *page = currentPage();

    if (!page->isShaderSourceModified()) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Shader"), "",
                        tr("GLSL Fragment shader (*.frag);; Text file (*.txt)"));

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);

    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Shader Workshop"),
                             tr("Could not write file %1:\n%2")
                                .arg(fileName)
                                .arg(file.errorString()));
        return;
    }

    QTextStream out(&file);

    out << page->shaderSource();
}
