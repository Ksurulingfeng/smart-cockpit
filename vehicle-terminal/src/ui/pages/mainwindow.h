#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "homePage.h"
#include "navigationPage.h"
#include "musicPage.h"
#include "settingsPage.h"
#include "toolsPage.h"
#include "rearViewCamera.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    HomePage *getHomePage()
    {
        return m_homePage;
    }
    NavigationPage *getNavigationPage()
    {
        return m_navigationPage;
    }
    MusicPage *getMusicPage()
    {
        return m_musicPage;
    }
    SettingsPage *getSettingsPage()
    {
        return m_settingsPage;
    }
    ToolsPage *getToolsPage()
    {
        return m_toolsPage;
    }

signals:

private:
    void setupPages();
    void setupIcons();
    void updateVolumeIcon(int volume);

    Ui::MainWindow *ui;
    HomePage *m_homePage                 = nullptr;
    NavigationPage *m_navigationPage     = nullptr;
    MusicPage *m_musicPage               = nullptr;
    SettingsPage *m_settingsPage         = nullptr;
    ToolsPage *m_toolsPage               = nullptr;
    RearViewCamera *m_rearViewCameraPage = nullptr;
};

#endif // MAINWINDOW_H