#include <QtGui/QFileDialog>
#include <QtGui/QStyleFactory>
#include <QClipboard>
#include <QTranslator>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QKeyEvent>

#include "mainwindow.h"

#include "settings.h"
#include "crqtutil.h"
#include "../crengine/include/lvtinydom.h"

#include "onyx/screen/screen_update_watcher.h"
#include "onyx/data/bookmark.h"
#include "onyx/ui/menu.h"
#include "onyx/sys/sys_status.h"
#include "onyx/ui/number_dialog.h"

#define DOC_CACHE_SIZE 128 * 0x100000

#ifndef ENABLE_BOOKMARKS_DIR
#define ENABLE_BOOKMARKS_DIR 1
#endif

using namespace ui;

OnyxMainWindow::OnyxMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    view_ = new CR3View;
    setCentralWidget(view_);

    status_bar_ = new StatusBar(this, MENU|PROGRESS|MESSAGE|CLOCK|BATTERY|SCREEN_REFRESH);
    setStatusBar(status_bar_);

    sys::SystemConfig conf;
    onyx::screen::watcher().addWatcher(this, conf.screenUpdateGCInterval());

#ifdef _LINUX
    QString homeDir = QDir::toNativeSeparators(QDir::homePath() + "/.cr3/");
#else
    QString homeDir = QDir::toNativeSeparators(QDir::homePath() + "/cr3/");
#endif

#ifdef _LINUX
    QString exeDir = QString(CR3_DATA_DIR);
#else
    QString exeDir = QDir::toNativeSeparators(qApp->applicationDirPath() + "/"); //QDir::separator();
#endif

    QString cacheDir = homeDir + "cache";
    QString bookmarksDir = homeDir + "bookmarks";
    QString histFile = exeDir + "cr3hist.bmk";
    QString histFile2 = homeDir + "cr3hist.bmk";
    QString iniFile = exeDir + "cr3.ini";
    QString iniFile2 = homeDir + "cr3.ini";
    QString cssFile = homeDir + "fb2.css";
    QString cssFile2 = exeDir + "fb2.css";

    QString hyphDir = exeDir + "hyph" + QDir::separator();
    ldomDocCache::init( qt2cr( cacheDir ), DOC_CACHE_SIZE );
    view_->setPropsChangeCallback( this );
    if ( !view_->loadSettings( iniFile ) )
        view_->loadSettings( iniFile2 );
    if ( !view_->loadHistory( histFile ) )
        view_->loadHistory( histFile2 );
    view_->setHyphDir( hyphDir );
    if ( !view_->loadCSS( cssFile ) )
        view_->loadCSS( cssFile2 );
#if ENABLE_BOOKMARKS_DIR==1
        view_->setBookmarksDir( bookmarksDir );
#endif

    QStringList args( qApp->arguments() );
    for ( int i=1; i<args.length(); i++ ) {
        if ( args[i].startsWith("--") ) {
            // option
        } else {
            // filename to open
            if ( file_name_to_open_.length()==0 )
                file_name_to_open_ = args[i];
        }
    }

    view_->loadDocument(file_name_to_open_);

    select_font_ = currentFont();
    font_family_actions_.loadExternalFonts();

#ifdef Q_WS_QWS
    connect(qApp->desktop(), SIGNAL(resized(int)), this, SLOT(onScreenSizeChanged(int)), Qt::QueuedConnection);
#endif
    connect( &(SysStatus::instance()), SIGNAL(aboutToShutdown()), this, SLOT(close()) );
    connect( &(SysStatus::instance()), SIGNAL(forceQuit()), this, SLOT(close()) );

    connect(status_bar_, SIGNAL(menuClicked()), this, SLOT(showContextMenu()));
    connect(view_, SIGNAL(updateProgress(int,int)), status_bar_, SLOT(setProgress(int,int)));
    connect(status_bar_, SIGNAL(progressClicked(int,int)), this ,SLOT(onProgressClicked(const int, const int)));
    connect(view_, SIGNAL(requestUpdateAll()), this,SLOT(updateScreen()));
    onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);

    view_->restoreWindowPos( this, "main.", true );
}

void OnyxMainWindow::closeEvent ( QCloseEvent * event )
{
    view_->saveWindowPos( this, "main." );
}

OnyxMainWindow::~OnyxMainWindow()
{
    delete status_bar_;
    delete view_;
}

void OnyxMainWindow::onPropsChange( PropsRef props )
{
    for ( int i=0; i<props->count(); i++ ) {
        QString name = props->name( i );
        QString value = props->value( i );
        int v = (value != "0");
        CRLog::debug("OnyxMainWindow::onPropsChange [%d] '%s'=%s ", i, props->name(i), props->value(i).toUtf8().data() );
        if ( name == PROP_WINDOW_FULLSCREEN ) {
            bool state = windowState().testFlag(Qt::WindowFullScreen);
            bool vv = v ? true : false;
            if ( state != vv )
                setWindowState( windowState() ^ Qt::WindowFullScreen );
        }
        if ( name == PROP_WINDOW_SHOW_MENU ) {
            //ui->menuBar->setVisible( v );
        }
        if ( name == PROP_WINDOW_SHOW_SCROLLBAR ) {
            //ui->scroll->setVisible( v );
        }
        if ( name == PROP_BACKGROUND_IMAGE ) {
            lString16 fn = qt2cr(value);
            LVImageSourceRef img;
            if ( !fn.empty() && fn[0]!='[' ) {
                CRLog::debug("Background image file: %s", LCSTR(fn));
                LVStreamRef stream = LVOpenFileStream(fn.c_str(), LVOM_READ);
                if ( !stream.isNull() ) {
                    img = LVCreateStreamImageSource(stream);
                }
            }
            fn.lowercase();
            bool tiled = ( fn.pos(lString16("\\textures\\"))>=0 || fn.pos(lString16("/textures/"))>=0);
        }
        if ( name == PROP_WINDOW_STYLE ) {
            QApplication::setStyle( value );
        }
    }
}


static bool firstShow = true;

void OnyxMainWindow::showEvent ( QShowEvent * event )
{
    if ( !firstShow )
        return;
    CRLog::debug("first showEvent()");
    firstShow = false;
}

static bool firstFocus = true;

void OnyxMainWindow::focusInEvent ( QFocusEvent * event )
{
    if ( !firstFocus )
        return;
    CRLog::debug("first focusInEvent()");
//    int n = view_->getOptions()->getIntDef( PROP_APP_START_ACTION, 0 );
//    if ( n==1 ) {
//        // show recent books dialog
//        CRLog::info("Startup Action: Show recent books dialog");
//        RecentBooksDlg::showDlg( view_ );
//    }

    firstFocus = false;
}


void OnyxMainWindow::on_actionFindText_triggered()
{
}

void OnyxMainWindow::keyPressEvent(QKeyEvent *ke)
 {
     ke->accept();
 }

 void OnyxMainWindow::keyReleaseEvent(QKeyEvent *ke)
 {
     switch (ke->key())
     {
     case Qt::Key_PageDown:
     case Qt::Key_Right:
         {
             view_->nextPageWithTTSChecking();
             return;
         }
         break;
     case Qt::Key_PageUp:
     case Qt::Key_Left:
         {
             view_->prevPageWithTTSChecking();
             return;
         }
         break;
     case Qt::Key_Up:
         {
             view_->zoomIn();
             onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
             return;
         }
     case Qt::Key_Down:
         {
             view_->zoomOut();
             onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
             return;
         }
     case Qt::Key_Enter: // fall through
     case Qt::Key_Return:
         {
             this->gotoPage();
             return;
         }
         break;
     case Qt::Key_Menu:
         {
             showContextMenu();
         }
         break;
     case Qt::Key_Escape:
         {
             if (this->isFullScreenByWidgetSize())
             {
                 status_bar_->show();
                 onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU,
                         onyx::screen::ScreenCommand::WAIT_NONE);
             }
             else
             {
                 this->close();
             }
         }
         break;
     }

     QMainWindow::keyReleaseEvent(ke);
 }

void OnyxMainWindow::showContextMenu()
{
    PopupMenu menu(this);
    updateActions();

    menu.addGroup(&font_family_actions_);
    menu.addGroup(&font_actions_);
    menu.addGroup(&reading_style_actions_);
    //menu.addGroup(&zoom_setting_actions_);
    menu.addGroup(&reading_tool_actions_);
    menu.setSystemAction(&system_actions_);

    if (menu.popup() != QDialog::Accepted)
    {
        //onyx::screen::instance().flush(0, onyx::screen::ScreenProxy::GC);
        QApplication::processEvents();
        return;
    }

    QAction * group = menu.selectedCategory();
    if (group == zoom_setting_actions_.category())
    {
        //processZoomingActions();
    }
    else if (group == font_family_actions_.category())
    {
        select_font_.setFamily(font_family_actions_.selectedFont());
        props_ref_->setString( PROP_FONT_FACE, font_family_actions_.selectedFont() );
        view_->setOptions(props_ref_);
    }
    else if (group == font_actions_.category())
    {
        select_font_ = font_actions_.selectedFont();
        props_ref_->setString( PROP_FONT_SIZE, QString().setNum(select_font_.pointSize()));
        if (select_font_.bold()) {
            view_->toggleFontEmbolden();
        }
        view_->setOptions(props_ref_);
    }
    else if (group == reading_tool_actions_.category())
    {
        processToolActions();
        return;
    }
    else if (group == system_actions_.category())
    {
        SystemAction system = system_actions_.selected();
        if (system == RETURN_TO_LIBRARY)
        {
            qApp->quit();
        }
        else if (system == FULL_SCREEN)
        {
            status_bar_->hide();
            onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU,
                    onyx::screen::ScreenCommand::WAIT_NONE);
        }
        else if (system == EXIT_FULL_SCREEN)
        {
            status_bar_->show();
            onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU,
                    onyx::screen::ScreenCommand::WAIT_NONE);
        }
        else if (system == MUSIC)
        {
            // Start or show music player.
            onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
            this->update();
            sys::SysStatus::instance().requestMusicPlayer(sys::START_PLAYER);
        }
        else if (system == ROTATE_SCREEN)
        {
            SysStatus::instance().rotateScreen();
//            view_->update();
//            onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
//            this->update();
            return;
        }
    }
    else if(group == reading_style_actions_.category())
    {
        ReadingStyleType s = reading_style_actions_.selected();
        if (s >= STYLE_LINE_SPACING_8 && s <= STYLE_LINE_SPACING_20)
        {
            const unsigned int Percentage_Compensation = 80;
            // since (int)STYLE_LINE_SPACING_8's value is 0
            int line_height_percentage = ((int)s * 10) + Percentage_Compensation;
            this->setLineHeight(line_height_percentage);

            status_bar_->update();
            onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
            return;
        }
    }
    onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GC);
    this->update();
}

void OnyxMainWindow::setLineHeight(const unsigned int lineHeightPercentage)
{
    props_ref_->setInt(PROP_INTERLINE_SPACE, lineHeightPercentage);
    view_->setOptions(props_ref_);
    return;
}

void OnyxMainWindow::updateReadingStyleActions()
{
    int index;
    view_->getOptions()->getInt(PROP_INTERLINE_SPACE, index);
    index = STYLE_LINE_SPACING_8+((index-80)/10);
    reading_style_actions_.generateActions(static_cast<ReadingStyleType>(index));
}

void OnyxMainWindow::updateToolActions()
{
    // Reading tools of go to page and clock.
    reading_tool_actions_.clear();
    std::vector<ReadingToolsType> tools;

    tools.clear();
    tools.push_back(::ui::SEARCH_TOOL);
    if (SysStatus::instance().hasTouchScreen() ||
        sys::SysStatus::instance().isDictionaryEnabled())
    {
        tools.push_back(DICTIONARY_TOOL);
    }
    tools.push_back(::ui::TOC_VIEW_TOOL);
    reading_tool_actions_.generateActions(tools, true);

    if (SysStatus::instance().hasTouchScreen() ||
        SysStatus::instance().isTTSEnabled())
    {
        tools.clear();
        tools.push_back(::ui::TEXT_TO_SPEECH);
        reading_tool_actions_.generateActions(tools, true);
    }

    tools.clear();
    if( !view_->hasBookmark() )
    {
        tools.push_back(::ui::ADD_BOOKMARK);
    }
    else
    {
        tools.push_back(::ui::DELETE_BOOKMARK);
    }

    tools.push_back(::ui::SHOW_ALL_BOOKMARKS);
    reading_tool_actions_.generateActions(tools, true);

    tools.clear();
    tools.push_back(::ui::GOTO_PAGE);
    tools.push_back(::ui::CLOCK_TOOL);

    reading_tool_actions_.generateActions(tools, true);
}

bool OnyxMainWindow::updateActions()
{   
    updateReadingStyleActions();
    updateToolActions();

    // Font family.
    QFont font = currentFont();
    font_family_actions_.generateActions(font.family(), true);

    // size
    std::vector<int> size;
    font_actions_.generateActions(font, size, font.pointSize());

    std::vector<int> all;
    all.push_back(ROTATE_SCREEN);

    if (isFullScreenByWidgetSize())
    {
        all.push_back(EXIT_FULL_SCREEN);
    } else
    {
        all.push_back(FULL_SCREEN);
    }

    all.push_back(MUSIC);
    all.push_back(RETURN_TO_LIBRARY);
    system_actions_.generateActions(all);
    return true;
}

const QFont & OnyxMainWindow::currentFont()
{
    props_ref_ = view_->getOptions();
    QString family;
    QString size;
    props_ref_->getString(PROP_FONT_FACE, family);
    props_ref_->getString(PROP_FONT_SIZE, size);
    select_font_ = QFont(family, size.toInt());
    return select_font_;
}

bool OnyxMainWindow::isFullScreenByWidgetSize()
{
    return !status_bar_->isVisible();
}

void OnyxMainWindow::processToolActions()
{
    ReadingToolsType tool = reading_tool_actions_.selectedTool();
    switch (tool)
    {
    case ::ui::TOC_VIEW_TOOL:
        {
            showTableOfContents();
        }
        break;
    case ::ui::DICTIONARY_TOOL:
        {
            view_->startDictLookup();
        }
        break;
    case ::ui::SEARCH_TOOL:
        {
            view_->showSearchWidget();
        }
        break;

    case ::ui::TEXT_TO_SPEECH:
        {
            view_->startTTS();
        }
        break;

    case ::ui::ADD_BOOKMARK:
        addBookmark();
        view_->restoreWindowPos(this, "MyBookmark");
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
        break;
    case ::ui::DELETE_BOOKMARK:
        view_->deleteBookmark();
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
        break;
    case ::ui::SHOW_ALL_BOOKMARKS:
        showAllBookmarks();
        break;

    case ::ui::GOTO_PAGE:
        {
            gotoPage();
        }
        break;
    case ::ui::CLOCK_TOOL:
        {
            showClock();
        }
        break;
    default:
        break;
    }
}

void OnyxMainWindow::gotoPage()
{
    if (view_->getDocView()->getPageCount() <= 1)
        return;

    //onyx::screen::instance().updateWidget(0, onyx::screen::ScreenProxy::GU);
    //emit requestGotoPageDialog();
    //statusbar_->onMessageAreaClicked();
    //resetScrollBar();
    // Popup page dialog.
    NumberDialog dialog(0);
    if (dialog.popup(view_->getDocView()->getCurPage(), view_->getDocView()->getPageCount()) != QDialog::Accepted)
    {
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
        return;
    }

    int current = dialog.value();
    this->onProgressClicked(1, current);
}

void OnyxMainWindow::showClock()
{
    onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
    status_bar_->onClockClicked();
}

void OnyxMainWindow::updateScreen()
{
    view_->update();

    if (onyx::screen::instance().defaultWaveform() == onyx::screen::ScreenProxy::DW)
    {
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::DW);
    }
    else
    {
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
    }
}

void OnyxMainWindow::onProgressClicked(const int percentage, const int value)
{
    view_->gotoPageWithTTSChecking(value);
}

void OnyxMainWindow::showTableOfContents()
{
    std::vector<int> paragraphs;
    std::vector<int> pages;
    std::vector<QString> titles;
    std::vector<QString> paths;
    LVTocItem * root = this->view_->getToc();

    for ( int i=0; i<root->getChildCount(); i++ )
    {
        LVTocItem *n = root->getChild(i);
        paragraphs.push_back(n->getLevel());
        titles.push_back( cr2qt(n->getName()));
        paths.push_back(cr2qt(n->getPath()));
        pages.push_back(n->getPage());
        for ( int j=0; j<n->getChildCount(); j++ )
        {
            LVTocItem *m = n->getChild(j);
            paragraphs.push_back(m->getLevel());
            titles.push_back( cr2qt(m->getName()));
            paths.push_back(cr2qt(m->getPath()));
            pages.push_back(m->getPage());
        }
    }

    std::vector<QStandardItem *> ptrs;
    QStandardItemModel model;

    QStandardItem *parent = model.invisibleRootItem();
    for (size_t i = 0;i < paragraphs.size();++i)
    {
        QStandardItem *item= new QStandardItem(titles[i]);
        item->setData(paths[i],Qt::UserRole+100);
        item->setEditable(false);
        ptrs.push_back(item);

        // Get parent.
        parent = searchParent(i, paragraphs, ptrs, model);
        parent->appendRow(item);
    }

    TreeViewDialog dialog( this );
    dialog.setModel( &model);

    int ret = dialog.popup( tr("Table of Contents") );
    if (ret != QDialog::Accepted)
    {
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
        return;
    }

    QModelIndex index = dialog.selectedItem();
    if ( !index.isValid() )
    {
        return;
    }
    view_->goToXPointer(index.data(Qt::UserRole+100).toString());
}

QStandardItem * OnyxMainWindow::searchParent(const int index,
                                           std::vector<int> & entries,
                                           std::vector<QStandardItem *> & ptrs,
                                           QStandardItemModel &model)
{
    int indent = entries[index];
    for(int i = index - 1; i >= 0; --i)
    {
        if (entries[i] < indent)
        {
            return ptrs[i];
        }
    }
    return model.invisibleRootItem();
}

bool OnyxMainWindow::addBookmark()
{
    view_->createBookmark();
    return true;
}

void OnyxMainWindow::showAllBookmarks()
{
    QStandardItemModel model;
    QModelIndex selected = model.invisibleRootItem()->index();
    bookmarkModel(model, selected);

    TreeViewDialog bookmark_view(this);
    bookmark_view.setModel(&model);
    QVector<int> percentages;
    percentages.push_back(80);
    percentages.push_back(20);
    bookmark_view.tree().setColumnWidth(percentages);

    int ret = bookmark_view.popup(QCoreApplication::tr("Bookmarks"));
    // Returned from the bookmark view.
    onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GC);
    this->update();

    if (ret != QDialog::Accepted)
    {
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GU);
        return;
    }

    CRBookmark *pos = (CRBookmark *)(model.data(bookmark_view.selectedItem(), Qt::UserRole + 1).toInt());
    view_->goToBookmark(pos);
}

void OnyxMainWindow::bookmarkModel(QStandardItemModel & model,
                                   QModelIndex & selected)
{
    CRFileHistRecord * rec = view_->getDocView()->getCurrentFileHistRecord();
    if ( !rec )
        return;
    LVPtrVector<CRBookmark> & list( rec->getBookmarks() );
    model.setColumnCount(2);
    int row = 0;
    for(int i  = 0; i < list.length(); ++i, ++row)
    {
        QString t =cr2qt(view_->getDocView()->getPageText(true, list[i]->getBookmarkPage()));
        t.truncate(100);
        QStandardItem *title = new QStandardItem(t);
        title->setData((int)list[i]);
        title->setEditable(false);
        model.setItem(row, 0, title);

        int pos = list[i]->getPercent();
        QString str(tr("%1"));
        str = str.arg(pos);
        QStandardItem *page = new QStandardItem(str);
        page->setTextAlignment(Qt::AlignCenter);
        page->setEditable(false);
        model.setItem(row, 1, page);
    }
}

void OnyxMainWindow::updateScreenManually()
{
    sys::SysStatus::instance().setSystemBusy(false);
    onyx::screen::instance().flush(this, onyx::screen::ScreenProxy::GC);
}

void OnyxMainWindow::onScreenSizeChanged(int)
{
    // Preserve updatability.
    QWidget *wnd = qApp->focusWidget();

    setFixedSize(qApp->desktop()->screenGeometry().size());

    if (isActiveWindow())
    {
        if (wnd)
        {
            wnd->setFocus();
        }
        update();
        onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GC);
    }

    onyx::screen::watcher().enqueue(this, onyx::screen::ScreenProxy::GC);
    this->update();

//    onyx::screen::instance().flush(this, onyx::screen::ScreenProxy::GC);
}
