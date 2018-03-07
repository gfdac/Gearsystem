/*
 * Gearsystem - Sega Master System / Game Gear Emulator
 * Copyright (C) 2013  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include <QFileDialog>
#include <QDesktopWidget>
#include <QSettings>
#include <QStandardPaths>
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "GLFrame.h"
#include "Emulator.h"
#include "InputSettings.h"
#include "SoundSettings.h"
#include "VideoSettings.h"
#include "About.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    qApp->installEventFilter(this);
    m_bFullscreen = false;
    m_iScreenSize = 2;
    m_iSelectedSlot = 1;

    m_bMenuPressed[0] = m_bMenuPressed[1] = m_bMenuPressed[2] = false;
    m_pUI = new Ui::MainWindow();
    m_pUI->setupUi(this);

    this->addAction(m_pUI->actionFullscreen);
    this->addAction(m_pUI->actionReset);
    this->addAction(m_pUI->actionPause);
    this->addAction(m_pUI->actionSave_State);
    this->addAction(m_pUI->actionLoad_State);

    this->setWindowTitle(GEARSYSTEM_TITLE);

    m_pExitShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_pExitShortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(m_pExitShortcut, SIGNAL(activated()), this, SLOT(Exit()));


    QObject::connect(m_pUI->menuGame_Boy, SIGNAL(aboutToShow()), this, SLOT(MenuGameBoyPressed()));
    QObject::connect(m_pUI->menuGame_Boy, SIGNAL(aboutToHide()), this, SLOT(MenuGameBoyReleased()));
    QObject::connect(m_pUI->menuDebug, SIGNAL(aboutToShow()), this, SLOT(MenuDebugPressed()));
    QObject::connect(m_pUI->menuDebug, SIGNAL(aboutToHide()), this, SLOT(MenuDebugReleased()));
    QObject::connect(m_pUI->menuSettings, SIGNAL(aboutToShow()), this, SLOT(MenuSettingsPressed()));
    QObject::connect(m_pUI->menuSettings, SIGNAL(aboutToHide()), this, SLOT(MenuSettingsReleased()));
    QObject::connect(m_pUI->menuHelp, SIGNAL(aboutToShow()), this, SLOT(MenuAboutPressed()));
    QObject::connect(m_pUI->menuHelp, SIGNAL(aboutToHide()), this, SLOT(MenuAboutReleased()));

    m_pUI->actionX_1->setData(1);
    m_pUI->actionX_2->setData(2);
    m_pUI->actionX_3->setData(3);
    m_pUI->actionX_4->setData(4);
    m_pUI->actionX_5->setData(5);

    m_pUI->action1->setData(1);
    m_pUI->action2->setData(2);
    m_pUI->action3->setData(3);
    m_pUI->action4->setData(4);
    m_pUI->action5->setData(5);

    m_pEmulator = new Emulator();
    m_pEmulator->Init();

    QGLFormat f;
    f.setSwapInterval(1);
    QGLFormat::setDefaultFormat(f);

    m_pGLFrame = new GLFrame();
    ResizeWindow(m_iScreenSize);
    setCentralWidget(m_pGLFrame);

    m_pInputSettings = new InputSettings(m_pGLFrame);

    m_pSoundSettings = new SoundSettings(m_pGLFrame, m_pEmulator);

    m_pVideoSettings = new VideoSettings(m_pGLFrame, m_pEmulator);

    m_pAbout = new About();

    QPalette pal = this->palette();
    pal.setColor(this->backgroundRole(), Qt::black);
    this->setPalette(pal);

    LoadSettings();

    m_pGLFrame->InitRenderThread(m_pEmulator);
}

MainWindow::~MainWindow()
{
    SaveSettings();

    SafeDelete(m_pAbout);
    SafeDelete(m_pExitShortcut);
    SafeDelete(m_pEmulator);
    SafeDelete(m_pGLFrame);
    SafeDelete(m_pInputSettings);
    SafeDelete(m_pSoundSettings);
    SafeDelete(m_pUI);
}

void MainWindow::Exit()
{
#ifdef DEBUG_SYSTEM
    m_pEmulator->MemoryDump();
#endif
    this->close();
}

void MainWindow::InitalGameBoyLoadROM(const char* szFilePath)
{
    if (IsValidPointer(szFilePath))
    {
        m_pGLFrame->PauseRenderThread();

        m_pEmulator->LoadRom(szFilePath, m_pUI->actionSave_RAM_in_ROM_folder->isChecked());
        m_pUI->actionPause->setChecked(false);

        setFocus();
        activateWindow();

        m_pGLFrame->ResumeRenderThread();
    }
}

void MainWindow::MenuGameBoyLoadROM()
{
    m_pGLFrame->PauseRenderThread();

    QString filename = QFileDialog::getOpenFileName(
            this,
            tr("Load ROM"),
            QDir::currentPath(),
            tr("Master System / Game Gear ROM files (*.sms *.gg *.zip);;All files (*.*)"));

    if (!filename.isNull())
    {
        m_pEmulator->LoadRom(filename.toUtf8().data(), m_pUI->actionSave_RAM_in_ROM_folder->isChecked());
        m_pUI->actionPause->setChecked(false);
    }

    setFocus();
    activateWindow();

    m_pGLFrame->ResumeRenderThread();
}

void MainWindow::MenuGameBoyPause()
{
    if (m_pEmulator->IsPaused())
        m_pEmulator->Resume();
    else
        m_pEmulator->Pause();
}

void MainWindow::MenuGameBoyReset()
{
    m_pUI->actionPause->setChecked(false);
    m_pEmulator->Reset(m_pUI->actionSave_RAM_in_ROM_folder->isChecked());
}

void MainWindow::MenuGameBoySelectStateSlot(QAction* action)
{
    m_pUI->action1->setChecked(false);
    m_pUI->action2->setChecked(false);
    m_pUI->action3->setChecked(false);
    m_pUI->action4->setChecked(false);
    m_pUI->action5->setChecked(false);
    action->setChecked(true);
    m_iSelectedSlot = action->data().toInt();
}

void MainWindow::MenuGameBoySaveState()
{
    m_pEmulator->SaveState(m_iSelectedSlot);
}

void MainWindow::MenuGameBoyLoadState()
{
    m_pEmulator->LoadState(m_iSelectedSlot);
}

void MainWindow::MenuSettingsInput()
{
    m_pGLFrame->PauseRenderThread();
    m_pInputSettings->show();
}

void MainWindow::MenuSettingsVideo()
{
    m_pGLFrame->PauseRenderThread();
    m_pVideoSettings->show();
}

void MainWindow::MenuSettingsSound()
{
    m_pGLFrame->PauseRenderThread();
    m_pSoundSettings->show();
}

void MainWindow::MenuSettingsWindowSize(QAction* action)
{
    m_pUI->actionX_1->setChecked(false);
    m_pUI->actionX_2->setChecked(false);
    m_pUI->actionX_3->setChecked(false);
    m_pUI->actionX_4->setChecked(false);
    m_pUI->actionX_5->setChecked(false);
    action->setChecked(true);
    m_iScreenSize = action->data().toInt();
    ResizeWindow(m_iScreenSize);
}

void MainWindow::MenuSettingsFullscreen()
{
    if (m_bFullscreen)
    {
        m_bFullscreen = false;
        this->showNormal();
        m_pUI->menubar->show();
        ResizeWindow(m_iScreenSize);
        m_pGLFrame->move(0, 0);
    }
    else
    {
        m_bFullscreen = true;

        this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        this->setMinimumSize(0, 0);
        this->showFullScreen();

        m_pUI->menubar->hide();

        int w = qApp->desktop()->size().width();
        int h = qApp->desktop()->size().height();

        int factor = h / GS_SMS_HEIGHT;

        m_pGLFrame->setMaximumSize(GS_SMS_WIDTH * factor, GS_SMS_HEIGHT * factor);
        m_pGLFrame->setMinimumSize(GS_SMS_WIDTH * factor, GS_SMS_HEIGHT * factor);

        int move_x = (w - (GS_SMS_WIDTH * factor)) / 2;
        int move_y = (h - (GS_SMS_HEIGHT * factor)) / 2;
        m_pGLFrame->setGeometry(move_x, move_y, GS_SMS_WIDTH * factor, GS_SMS_HEIGHT * factor);
    }

    setFocus();
    activateWindow();
}

void MainWindow::MenuSettingsSaveRAMInROMFolder()
{
}

void MainWindow::MenuDebugDisassembler()
{
}

void MainWindow::MenuDebugOAM()
{
}

void MainWindow::MenuDebugMap()
{
}

void MainWindow::MenuDebugPalette()
{
}

void MainWindow::MenuAbout()
{
    m_pAbout->setModal(true);
    m_pAbout->show();
}

void MainWindow::MenuGameBoyPressed()
{
    m_bMenuPressed[0] = true;
    m_pGLFrame->PauseRenderThread();
}

void MainWindow::MenuGameBoyReleased()
{
    m_bMenuPressed[0] = false;
    MenuReleased();
}

void MainWindow::MenuSettingsPressed()
{
    m_bMenuPressed[1] = true;
    m_pGLFrame->PauseRenderThread();
}

void MainWindow::MenuSettingsReleased()
{
    m_bMenuPressed[1] = false;
    MenuReleased();
}

void MainWindow::MenuDebugPressed()
{
    m_bMenuPressed[2] = true;
    m_pGLFrame->PauseRenderThread();
}

void MainWindow::MenuDebugReleased()
{
    m_bMenuPressed[2] = false;
    MenuReleased();
}

void MainWindow::MenuAboutPressed()
{
    m_bMenuPressed[3] = true;
    m_pGLFrame->PauseRenderThread();
}

void MainWindow::MenuAboutReleased()
{
    m_bMenuPressed[3] = false;
    MenuReleased();
}

void MainWindow::MenuReleased()
{
    for (int i = 0; i < 4; i++)
    {
        if (m_bMenuPressed[i])
            return;
    }
    m_pGLFrame->ResumeRenderThread();
}

void MainWindow::closeEvent(QCloseEvent *evt)
{
    m_pGLFrame->StopRenderThread();
    QMainWindow::closeEvent(evt);
}

void MainWindow::keyPressEvent(QKeyEvent* e)
{
    if (!e->isAutoRepeat())
    {
        GS_Joypads joypad = Joypad_1;
        switch (m_pInputSettings->GetKey(e->key()))
        {
            case 0:
                m_pEmulator->KeyPressed(joypad, Key_Up);
                break;
            case 3:
                m_pEmulator->KeyPressed(joypad, Key_Left);
                break;
            case 1:
                m_pEmulator->KeyPressed(joypad, Key_Right);
                break;
            case 2:
                m_pEmulator->KeyPressed(joypad, Key_Down);
                break;
            case 6:
                m_pEmulator->KeyPressed(joypad, Key_Start);
                break;
            case 5:
                m_pEmulator->KeyPressed(joypad, Key_2);
                break;
            case 4:
                m_pEmulator->KeyPressed(joypad, Key_1);
                break;
            default:
                break;
        }
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent* e)
{
    if (!e->isAutoRepeat())
    {
        GS_Joypads joypad = Joypad_1;
        switch (m_pInputSettings->GetKey(e->key()))
        {
            case 0:
                m_pEmulator->KeyReleased(joypad, Key_Up);
                break;
            case 3:
                m_pEmulator->KeyReleased(joypad, Key_Left);
                break;
            case 1:
                m_pEmulator->KeyReleased(joypad, Key_Right);
                break;
            case 2:
                m_pEmulator->KeyReleased(joypad, Key_Down);
                break;
            case 6:
                m_pEmulator->KeyReleased(joypad, Key_Start);
                break;
            case 5:
                m_pEmulator->KeyReleased(joypad, Key_2);
                break;
            case 4:
                m_pEmulator->KeyReleased(joypad, Key_1);
                break;
            default:
                break;
        }
    }
}

void MainWindow::ResizeWindow(int factor)
{
    m_iScreenSize = factor;
    m_pGLFrame->setMaximumSize(GS_SMS_WIDTH * factor, GS_SMS_HEIGHT * factor);
    m_pGLFrame->setMinimumSize(GS_SMS_WIDTH * factor, GS_SMS_HEIGHT * factor);
}

bool MainWindow::eventFilter(QObject * watched, QEvent * event)
{
    if (event->type() == QEvent::ApplicationActivate)
    {
        m_pGLFrame->ResumeRenderThread();
    }
    else if (event->type() == QEvent::ApplicationDeactivate)
    {
        m_pGLFrame->PauseRenderThread();
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::LayoutRequest)
    {
        if (!m_bFullscreen)
        {
            this->setMaximumSize(sizeHint());
            this->setMinimumSize(sizeHint());
            this->resize(sizeHint());
        }
    }

    return QMainWindow::event(ev);
}

void MainWindow::LoadSettings()
{
    QSettings settings("gearsystem.ini", QSettings::IniFormat);

    settings.beginGroup("Gearsystem");
    m_iSelectedSlot = settings.value("SavestateSlot", 1).toInt();

    switch (m_iSelectedSlot)
    {
        case 1:
            MenuGameBoySelectStateSlot(m_pUI->action1);
            break;
        case 2:
            MenuGameBoySelectStateSlot(m_pUI->action2);
            break;
        case 3:
            MenuGameBoySelectStateSlot(m_pUI->action3);
            break;
        case 4:
            MenuGameBoySelectStateSlot(m_pUI->action4);
            break;
        case 5:
            MenuGameBoySelectStateSlot(m_pUI->action5);
            break;
    }

    m_iScreenSize = settings.value("ScreenSize", 2).toInt();

    switch (m_iScreenSize)
    {
        case 1:
            MenuSettingsWindowSize(m_pUI->actionX_1);
            break;
        case 2:
            MenuSettingsWindowSize(m_pUI->actionX_2);
            break;
        case 3:
            MenuSettingsWindowSize(m_pUI->actionX_3);
            break;
        case 4:
            MenuSettingsWindowSize(m_pUI->actionX_4);
            break;
        case 5:
            MenuSettingsWindowSize(m_pUI->actionX_5);
            break;
    }

    m_bFullscreen = !settings.value("FullScreen", false).toBool();

    MenuSettingsFullscreen();

    m_pUI->actionSave_RAM_in_ROM_folder->setChecked(settings.value("SaveInROMFolder", false).toBool());
    settings.endGroup();

    settings.beginGroup("Input");
    m_pInputSettings->LoadSettings(settings);
    settings.endGroup();
    settings.beginGroup("Video");
    m_pVideoSettings->LoadSettings(settings);
    settings.endGroup();
    settings.beginGroup("Sound");
    m_pSoundSettings->LoadSettings(settings);
    settings.endGroup();
}

void MainWindow::SaveSettings()
{
    QSettings settings("gearsystem.ini", QSettings::IniFormat);

    settings.beginGroup("Gearsystem");
    settings.setValue("SavestateSlot", m_iSelectedSlot);
    settings.setValue("ScreenSize", m_iScreenSize);
    settings.setValue("FullScreen", m_bFullscreen);
    settings.setValue("SaveInROMFolder", m_pUI->actionSave_RAM_in_ROM_folder->isChecked());
    settings.endGroup();

    settings.beginGroup("Input");
    m_pInputSettings->SaveSettings(settings);
    settings.endGroup();
    settings.beginGroup("Video");
    m_pVideoSettings->SaveSettings(settings);
    settings.endGroup();
    settings.beginGroup("Sound");
    m_pSoundSettings->SaveSettings(settings);
    settings.endGroup();
}
