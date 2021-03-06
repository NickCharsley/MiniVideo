/*!
 * COPYRIGHT (C) 2014 Emeric Grange - All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \file      mainwindow.h
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2014
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
/* ************************************************************************** */

// minivideo library
#include <minivideo.h>

#include <QMainWindow>
#include <vector>

/* ************************************************************************** */

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public slots:
    void loadFileDialog();
    void About();
    void AboutQt();

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void dropEvent(QDropEvent *ev);
    void dragEnterEvent(QDragEnterEvent *ev);

private slots:
    int printDatas(int fileIndex);
    int loadFile(const QString &file);
    void closeFile();
    void hideStatus();

private:
    Ui::MainWindow *ui;

    QTimer *statusTimer;

    bool emptyFileList;
    std::vector <VideoFile_t *> videosList;

    void setStatus(const QString &text, int type, int duration = 0);
    int getDatas(const QString &file);
    void cleanDatas();
};

/* ************************************************************************** */
#endif // MAINWINDOW_H
