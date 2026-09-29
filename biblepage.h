/*
 * Copyright (C) 2026 Michael Speckenbach
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "biblepassage.h"
#include "biblestore.h"

#include <QHash>
#include <QTimer>
#include <QWidget>
#include <memory>

class QButtonGroup;
class QComboBox;
class QLabel;
class QListWidget;
class QStackedLayout;
class QToolButton;
class QVBoxLayout;

// Page of the main window's stacked widget to choose a bible passage:
//   translation -> book (tiles grouped by genre) -> chapter (tiles) -> verses (tiles + text).
// Verse tiles and verse text are coupled; both support click, drag, Shift+click and Ctrl+click.
class BiblePage : public QWidget {
    Q_OBJECT

public:
    explicit BiblePage(QWidget *parent = nullptr);
    ~BiblePage() override;

    void reloadBibles();                          // call when the page is shown (bibles may have changed)
    void startNew();                              // new selection, keeps translation, book and chapter
    void setPassage(const BiblePassage &passage); // edit an existing entry

    // Current selection; no verse selected = whole chapter. Invalid if no chapter is chosen.
    BiblePassage passage() const;

signals:
    void selectionChanged();
    void settingsRequested();   // "no bible installed" -> open the settings

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void buildBookTiles();
    void openBible(int comboIndex);
    void selectBook(int book, int chapter = 1);
    void loadChapter(int chapter);
    void syncSelection(QListWidget *from, QListWidget *to);
    void updateSummary();
    void updatePreview();
    static void fitTileList(QListWidget *list);

    std::unique_ptr<Bible> m_bible;
    int  m_book = 0;
    int  m_chapter = 0;
    bool m_syncing = false;

    QStackedLayout *m_stack = nullptr;
    QWidget        *m_emptyPage = nullptr;
    QWidget        *m_browserPage = nullptr;

    QComboBox    *m_bibleCombo = nullptr;
    QWidget      *m_bookContainer = nullptr;
    QVBoxLayout  *m_bookLayout = nullptr;
    QButtonGroup *m_bookGroup = nullptr;
    QHash<int, QToolButton *> m_bookButtons;

    QLabel      *m_bookTitle = nullptr;
    QListWidget *m_chapterList = nullptr;
    QListWidget *m_verseTiles = nullptr;
    QListWidget *m_verseText = nullptr;

    QLabel      *m_summary = nullptr;
    QComboBox   *m_splitCombo = nullptr;
    QListWidget *m_previewList = nullptr;
    QTimer       m_previewTimer;
};
