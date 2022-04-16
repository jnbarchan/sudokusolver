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
#include <QUndoStack>
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
    void loadFile(const QString &filePath);

public slots:
    void initialLoad(const QString &fileName);

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
    struct FlashPossibilities
    {
        QModelIndex index;
        int num;
        FlashPossibilities(const QModelIndex &index, int num)
        {
            this->index = index;
            this->num = num;
        }
    };

    BoardModel(QObject *parent = nullptr);

    QUndoStack undoStack;
    void clearBoard();
    bool isSolved() const;
    bool checkForDuplicates();
    bool checkForNoPossibilities() const;
    void loadBoard(QTextStream &ts);
    void saveBoard(QTextStream &ts) const;
    void solveStart();
    CellNum solveStep();
    bool numIsPossible(int num, const QModelIndex &index) const;

    const QModelIndex &flashCellIndex() { return _flashCellIndex; }
    const QList<FlashPossibilities> &flashPossibilities() { return _flashPossibilities; };
    void stopFlashing();
    void startFlashing();
    void markFlashPossibilitiesAsChanged();

    void doSetDataUndoCommand(const QModelIndex &index, const QVariant &oldValue, const QVariant &newValue);

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
        int groupIndex;

        static int paramForDirection(CellGroupIteratorDirection direction, int row, int col);
        static void rowColForIndexInSquare(int index, int square, int &row, int &col);
        CellGroupIterator(CellGroupIteratorDirection direction, int param);
        CellGroupIterator(CellGroupIteratorDirection direction, int row, int col);
        bool atEnd() const;
        bool next();
    };

    class SetDataUndoCommand : public QUndoCommand
    {
    public:
        SetDataUndoCommand(BoardModel *board, const QModelIndex &index, const QVariant &oldValue, const QVariant &newValue);

    private:
        BoardModel *board;
        QModelIndex index;
        QVariant oldValue, newValue;

    protected:
        virtual void redo() override;
        virtual void undo() override;
    };

    bool possibilities[9][9][10];
    bool possibilitiesInitialised;

    QModelIndex _flashCellIndex;
    QList<FlashPossibilities> _flashPossibilities;

    void setPossibility(int row, int col, int num, bool possible);
    void resetAllPossibilities();
    void reducePossibilities(int row, int col);
    void reduceAllPossibilities();
    void clearAllData();
    int numInCell(int row, int col) const;
    bool numInCellHasDuplicate(int row, int col) const;
    bool cellHasOnePossibility(int row, int col, int &num) const;
    CellNum solveFindStepPass1() const;
    CellNum cellGroupOnlyPossibilityForNum(CellGroupIteratorDirection direction, int param) const;
    CellNum solveFindStepPass2() const;
    QList<int> numPossibilitiesListForCell(int row, int col) const;
    QVector<QList<int> > cellGroupPossibilitiesByIndex(CellGroupIteratorDirection direction, int param) const;
    bool reduceCellGroupPossibilitiesForIdenticalPairs(CellGroupIteratorDirection direction, int param);
    bool reduceAllGroupPossibilitiesForIdenticalPairs();
    QList<int> groupIndexPossibilitiesListForNumber(CellGroupIteratorDirection direction, int param, int num) const;
    QVector<QList<int> > cellGroupPossibilitiesByNumber(CellGroupIteratorDirection direction, int param) const;
    bool reduceCellGroupPossibilitiesForUniquePairs(CellGroupIteratorDirection direction, int param);
    bool reduceAllGroupPossibilitiesForUniquePairs();
    bool reduceRowColumnPossibilitiesForSquare(int param);
    bool reduceAllRowColumnPossibilitiesForSquares();
    CellNum solveFindStepPass3();
    CellNum solveFindStep();

signals:
    void beginFlashing();
    void endFlashing();

public slots:
    void modelDataEdited(const QModelIndex &index, int num);
};


////////// CLASS BoardView //////////
class BoardView : public QTableView
{
    Q_OBJECT

public:
    BoardView(QWidget *parent = nullptr);

    void setShowPossibilities(bool show);
    bool flashHide() const;
    virtual void setModel(QAbstractItemModel *model) override;

private:
    struct CellFlasher
    {
        QTimer timer;
        int countdown;

        CellFlasher()
        {
            timer.setInterval(4000);
            countdown = 0;
        }
    };

    BoardModel *boardModel;
    CellFlasher cellFlasher;
    BoardCellDelegate *cellDelegate;

signals:
    void modelDataEdited(const QModelIndex &index, int num) const;

public slots:
    void modelBeginFlashing();
    void modelEndFlashing();

private slots:
    void cellFlasherTimeout();
};


////////// CLASS BoardCellDelegate //////////
class BoardCellDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    BoardCellDelegate(BoardView *boardViewParent);
    bool showPossibilities() const;
    void setShowPossibilities(bool show);
    void setBoardModel(BoardModel *boardModel);

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

private:
    BoardView *boardView;
    BoardModel *boardModel;
    bool _showPossibilities;

    bool isNumToBeFlashed(int num, const QModelIndex &index, const QList<BoardModel::FlashPossibilities> &fps) const;

signals:
    void modelDataEdited(const QModelIndex &index, int num) const;
};
#endif // MAINWINDOW_H
