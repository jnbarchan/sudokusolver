#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDebug>
#include <QList>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <QVector>

class BoardModel;
class BoardView;
class BoardCellDelegate;

struct CellNum
{
    int row, col;
    int num;

    CellNum() { row = col = num = 0; }
    CellNum(int row, int col, int num)
    {
        this->row = row;
        this->col = col;
        this->num = num;
    }
    bool isEmpty() const { return (num == 0); };
};

////////// CLASS MainWindow //////////
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    BoardModel *board;
    BoardView *boardView;

private:
    QAction *showPossibilitiesAction;
    QString saveDirectory() const;

private slots:
    void actionClear();
    void actionLoad();
    void actionSave();
    void actionExit();
    void actionShowPossibilities();
    void actionSolveStart();
    void actionSolveStep();
};


////////// CLASS BoardModel //////////
class BoardModel : public QStandardItemModel
{
    Q_OBJECT

public:
    BoardModel(QObject *parent = nullptr);

    void clearBoard();
    bool isSolved() const;
    bool checkForDuplicates();
    void loadBoard(QTextStream &ts);
    void saveBoard(QTextStream &ts) const;
    void solveStart();
    CellNum solveStep();
    bool numIsPossible(int num, const QModelIndex &index) const;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

private:
    enum CellGroupIteratorDirection { Row, Column, Square };
    struct CellGroupIterator
    {
        CellGroupIteratorDirection direction;
        int row0, col0;
        int row, col;

        CellGroupIterator(CellGroupIteratorDirection direction, int param);
//        {
//            this->direction = direction;
//            row0 = col0 = 0;
//            switch (direction)
//            {
//            case Row: row0 = param; break;
//            case Column: col0 = param; break;
//            case Square:
//                row0 = param / 3;
//                col0 = param % 3;
//                break;
//            }
//            row = row0;
//            col = col0;
//        }

        bool atEnd() const;
//        {
//            switch (direction)
//            {
//            case Row: return (col >= 9);
//            case Column: return (row >= 9);
//            case Square: return (row >= row0 + 3 || (row == row0 + 2 && col >= col0 + 3));
//            }
//            return true;
//        }

        bool next();
//        {
//            if (atEnd())
//                return false;
//            switch (direction)
//            {
//            case Row: col++; break;
//            case Column: row++; break;
//            case Square:
//                if (++col >= col0 + 3)
//                {
//                    col = col0;
//                    row++;
//                }
//                break;
//            }
//            return true;
//        }
    };

    bool possibilities[9][9][10];
    bool possibilitiesInitialised;

    void setPossibility(int row, int col, int num, bool possible);
    void resetAllPossibilities();
    void reducePossibilities(int row, int col);
    void reduceAllPossibilities();
    void clearAllData();
    bool numInCellHasDuplicate(int row, int col) const;
    bool solveCellHasOnePossibility(int row, int col, int &num) const;
    CellNum solveFindStepPass1() const;
    CellNum cellGroupOnlyPossibilityForNum(int row, int col, int square) const;
    CellNum solveFindStepPass2() const;
    QList<int> numPossibilitiesList(int row, int col) const;
    QVector<QList<int> > solveCellGroupPossibilities(int row, int col, int square) const;
    bool findCellGroupIndexesForIdenticalPairs(const QVector<QList<int> > &groupPossibilities, int &cell1, int &cell2) const;
    bool reduceCellGroupPossibilitiesForIdenticalPairs(int row, int col, int square);
    CellNum solveFindStepPass3();
    CellNum solveFindStep();

public slots:
    void modelDataEdited();
};


////////// CLASS BoardView //////////
class BoardView : public QTableView
{
    Q_OBJECT

public:
    BoardView(QWidget *parent = nullptr);

    void setShowPossibilities(bool show);
    void flashCell(const QModelIndex &index);
    virtual void setModel(QAbstractItemModel *model) override;

private:
    struct CellFlasher
    {
        QTimer timer;
        QModelIndex index;
        int countdown;

        CellFlasher()
        {
            timer.setInterval(4000);
            index = QModelIndex();
            countdown = 0;
        }
    };

    CellFlasher cellFlasher;
    BoardCellDelegate *cellDelegate;

signals:
    void modelDataEdited(const QModelIndex &index) const;

private slots:
    void cellFlasherTimeout();
};


////////// CLASS BoardCellDelegate //////////
class BoardCellDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    BoardCellDelegate(QObject *parent = nullptr);
    bool showPossibilities() const;
    void setShowPossibilities(bool show);
    void setBoardModel(BoardModel *boardModel);

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

private:
    bool _showPossibilities;
    BoardModel *boardModel;

signals:
    void modelDataEdited(const QModelIndex &index) const;
};
#endif // MAINWINDOW_H
