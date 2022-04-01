#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>

#include <stdexcept>

#include "mainwindow.h"


////////// CLASS MainWindow //////////

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Sudoku Solver");
    resize(800, 800);

    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Clear", this, &MainWindow::actionClear);
    fileMenu->addAction("&Load", this, &MainWindow::actionLoad);
    fileMenu->addAction("&Save", this, &MainWindow::actionSave);
    fileMenu->addAction("E&xit", this, &MainWindow::actionExit);

    QMenu *solveMenu = menuBar()->addMenu("&Solve");
    showPossibilitiesAction = solveMenu->addAction("Sho&w Possibilities", this, &MainWindow::actionShowPossibilities);
    showPossibilitiesAction->setCheckable(true);
    solveMenu->addAction("St&art", this, &MainWindow::actionSolveStart);
    solveMenu->addAction("St&ep", this, &MainWindow::actionSolveStep, QKeySequence(Qt::CTRL + Qt::Key_E));

    board = new BoardModel(this);
    boardView = new BoardView(this);
    boardView->setModel(board);
    connect(boardView, &BoardView::modelDataEdited, board, &BoardModel::modelDataEdited);

    this->setCentralWidget(boardView);
}

MainWindow::~MainWindow()
{
}

QString MainWindow::saveDirectory() const
{
    return QCoreApplication::applicationDirPath() + "/../sudokusolver/saves";
}

/*slot*/ void MainWindow::actionClear()
{
    board->clearBoard();
}

/*slot*/ void MainWindow::actionLoad()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File", saveDirectory());
    if (fileName.isNull())
        return;
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Failed to Open File", f.errorString());
        return;
    }
    QTextStream ts(&f);

    try {
        board->loadBoard(ts);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Error reading file", e.what());
        return;
    }
}

/*slot*/ void MainWindow::actionSave()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save File", saveDirectory());
    if (fileName.isNull())
        return;
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Failed to Save File", f.errorString());
        return;
    }
    QTextStream ts(&f);
    board->saveBoard(ts);
}

/*slot*/ void MainWindow::actionExit()
{
    qApp->quit();
}

/*slot*/ void MainWindow::actionShowPossibilities()
{
    boardView->setShowPossibilities(showPossibilitiesAction->isChecked());
}

/*slot*/ void MainWindow::actionSolveStart()
{
    showPossibilitiesAction->setChecked(true);
    actionShowPossibilities();
    board->solveStart();
}

/*slot*/ void MainWindow::actionSolveStep()
{
    showPossibilitiesAction->setChecked(true);
    actionShowPossibilities();
    CellNum cellNum = board->solveStep();
    if (cellNum.isEmpty())
    {
        QString message("No move could be found");
        if (board->checkForDuplicates())
            message += " (board is illegal/has duplicates)";
        else if (board->isSolved())
            message += " (board is solved)";
        else
            message += " (cannot find any move which is certain)";
        QMessageBox::warning(this, "No Move", message);
        return;
    }
    boardView->flashCell(board->index(cellNum.row, cellNum.col));
}


////////// CLASS BoardModel //////////

BoardModel::BoardModel(QObject *parent /*= nullptr*/)
    : QStandardItemModel(9, 9, parent)
{
    resetAllPossibilities();
}

///// STRUCT CellGroupIterator /////

/*static*/ int BoardModel::CellGroupIterator::paramForDirection(BoardModel::CellGroupIteratorDirection direction, int row, int col)
{
    switch (direction)
    {
    case Row: return row;
    case Column: return col;
    case Square: return (row / 3) * 3 + (col / 3);
    }
    return -1;
}

BoardModel::CellGroupIterator::CellGroupIterator(CellGroupIteratorDirection direction, int param)
{
    this->direction = direction;
    row0 = col0 = groupIndex = 0;
    switch (direction)
    {
    case Row: row0 = param; break;
    case Column: col0 = param; break;
    case Square:
        row0 = param / 3 * 3;
        col0 = param % 3 * 3;
        break;
    }
    row = row0;
    col = col0;
}

BoardModel::CellGroupIterator::CellGroupIterator(CellGroupIteratorDirection direction, int row, int col)
    : CellGroupIterator(direction, paramForDirection(direction, row, col))
{
}

bool BoardModel::CellGroupIterator::atEnd() const
{
    switch (direction)
    {
    case Row: return (col >= 9);
    case Column: return (row >= 9);
    case Square: return (row >= row0 + 3 || (row == row0 + 2 && col >= col0 + 3));
    }
    return true;
}

bool BoardModel::CellGroupIterator::next()
{
    if (atEnd())
        return false;
    groupIndex++;
    switch (direction)
    {
    case Row: col++; break;
    case Column: row++; break;
    case Square:
        if (++col >= col0 + 3)
        {
            col = col0;
            row++;
        }
        break;
    }
    return true;
}


void BoardModel::setPossibility(int row, int col, int num, bool possible)
{
    if (possibilities[row][col][num] == possible)
        return;
    possibilities[row][col][num] = possible;
    QModelIndex ix(index(row, col));
    emit dataChanged(ix, ix);
}

void BoardModel::resetAllPossibilities()
{
    for (int row = 0; row < 9; row++)
        for (int col = 0; col < 9; col++)
            for (int num = 1; num <= 9; num++)
                possibilities[row][col][num] = true;
    possibilitiesInitialised = false;
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void BoardModel::reducePossibilities(int row, int col)
{
    int numHere = numInCell(row, col);
    if (numHere == 0)
        return;
    for (int num = 1; num <= 9; num++)
        setPossibility(row, col, num, false);
    for (CellGroupIteratorDirection direction : {Column, Row, Square})
        for (CellGroupIterator cgit(direction, row, col); !cgit.atEnd(); cgit.next())
            setPossibility(cgit.row, cgit.col, numHere, false);
}

void BoardModel::reduceAllPossibilities()
{
    for (int row = 0; row < 9; row++)
        for (int col = 0; col < 9; col++)
            reducePossibilities(row, col);
    possibilitiesInitialised = true;
}

void BoardModel::clearAllData()
{
    for (int row = 0; row < rowCount(); row++)
        for (int col = 0; col < columnCount(); col++)
            clearItemData(index(row, col));
    resetAllPossibilities();
}

void BoardModel::clearBoard()
{
    beginResetModel();
    clearAllData();
    endResetModel();
}

int BoardModel::numInCell(int row, int col) const
{
    return index(row, col).data().toInt();
}

bool BoardModel::isSolved() const
{
    for (int row = 0; row < rowCount(); row++)
        for (int col = 0; col < columnCount(); col++)
            if (numInCell(row, col) == 0)
                return false;
    return true;
}

bool BoardModel::numInCellHasDuplicate(int row, int col) const
{
    int num = numInCell(row, col);
    if (num == 0)
        return false;
    for (CellGroupIteratorDirection direction : {Column, Row, Square})
        for (CellGroupIterator cgit(direction, row, col); !cgit.atEnd(); cgit.next())
            if (!(cgit.row == row && cgit.col == col) && numInCell(cgit.row, cgit.col) == num)
                return true;
    return false;
}

bool BoardModel::checkForDuplicates()
{
    bool anyDuplicate = false;
    for (int row = 0; row < 9; row++)
        for (int col = 0; col < 9; col++)
        {
            bool duplicate = numInCellHasDuplicate(row, col);
            setData(index(row, col), duplicate ? QColor(Qt::red) : QVariant(), Qt::ForegroundRole);
            anyDuplicate |= duplicate;
        }
    return anyDuplicate;
}

void BoardModel::loadBoard(QTextStream &ts)
{
    beginResetModel();
    clearAllData();
    try {
        int row = 0;
        while (!ts.atEnd())
        {
            if (row >= rowCount())
                throw std::runtime_error("Too many lines in file");
            QString line = ts.readLine();
            QStringList nums = line.split(" ", QString::SkipEmptyParts);
            if (nums.count() != columnCount())
                throw std::runtime_error("Incorrect number of elements in line");
            for (int col = 0; col < columnCount(); col++)
            {
                bool ok;
                int num = nums[col].toInt(&ok);
                if (ok)
                    ok = (num >= 0 && num <= 9);
                if (!ok)
                    throw std::runtime_error("Bad number in file");
                if (!setData(index(row, col), (num != 0) ? num : QVariant()))
                    throw std::runtime_error("Failed to set data for element in line");
            }
            row++;
        }
        if (row != rowCount())
            throw std::runtime_error("Too few lines in file");
    } catch (const std::exception &e) {
        endResetModel();
        throw;
    }
    checkForDuplicates();
    endResetModel();
}

void BoardModel::saveBoard(QTextStream &ts) const
{
    for (int row = 0; row < rowCount(); row++)
    {
        for (int col = 0; col < columnCount(); col++)
            ts << numInCell(row, col) << " ";
        ts << endl;
    }
}

void BoardModel::solveStart()
{
    resetAllPossibilities();
    reduceAllPossibilities();
}

bool BoardModel::cellHasOnePossibility(int row, int col, int &num) const
{
    num = 0;
    if (numInCell(row, col) != 0)
        return false;
    int found = 0;
    for (int num1 = 1; num1 <= 9; num1++)
        if (possibilities[row][col][num1])
        {
            if (++found > 1)
                return false;
            num = num1;
        }
    return (found == 1);
}

CellNum BoardModel::solveFindStepPass1() const
{
    // find if there is a cell which has just 1 possibility available
    int num;
    for (int row = 0; row < 9; row++)
        for (int col = 0; col < 9; col++)
            if (cellHasOnePossibility(row, col, num))
                return CellNum(row, col, num);
    return CellNum();
}

CellNum BoardModel::cellGroupOnlyPossibilityForNum(CellGroupIteratorDirection direction, int param) const
{
    for (CellGroupIterator cgit(direction, param); !cgit.atEnd(); cgit.next())
        if (numInCell(cgit.row, cgit.col) == 0)
            for (int num = 1; num <= 9; num++)
                if (possibilities[cgit.row][cgit.col][num])
                {
                    int found = 0;
                    for (CellGroupIterator cgit2(direction, param); !cgit2.atEnd(); cgit2.next())
                        if (numInCell(cgit2.row, cgit2.col) == 0 && possibilities[cgit2.row][cgit2.col][num])
                            found++;
                    if (found == 1)
                        return CellNum(cgit.row, cgit.col, num);
                }
    return CellNum();
}

CellNum BoardModel::solveFindStepPass2() const
{
    // find if there is a "group" (row/column/square) of cells
    // where there is some possibility which is only available *once* in the group
    CellNum cellNum;
    for (CellGroupIteratorDirection direction : {Column, Row, Square})
        for (int param = 0; param < 9; param++)
            if (!(cellNum = cellGroupOnlyPossibilityForNum(direction, param)).isEmpty())
                return cellNum;
    return CellNum();
}

QList<int> BoardModel::numPossibilitiesListForCell(int row, int col) const
{
    // return a list of which numbers are possible in cell (row,col)
    QList<int> nums;
    if (numInCell(row, col) != 0)
        return nums;
    for (int num = 1; num <= 9; num++)
        if (possibilities[row][col][num])
            nums.append(num);
    return nums;
}

QVector<QList<int> > BoardModel::cellGroupPossibilitiesByIndex(CellGroupIteratorDirection direction, int param) const
{
    // return an array indexed by "group" element index
    // where each array element is a list of which numbers are possible in the group element index
    QVector<QList<int> > groupVec(9);
    for (CellGroupIterator cgit(direction, param); !cgit.atEnd(); cgit.next())
        groupVec[cgit.groupIndex] = numPossibilitiesListForCell(cgit.row, cgit.col);
    return groupVec;
}

bool BoardModel::findCellGroupIndexesForIdenticalPairs(const QVector<QList<int> > &groupPossibilities, int &cell1, int &cell2) const
{
    // return whether, by looking through the array of lists of possible numbers in each element of a group,
    // there are any 2 elements which have just 2 possible numbers *and* those numbers are the same in both elements
    // in that case, set `cell1` & `cell2` to the group indexes of the identical pair
    Q_ASSERT(groupPossibilities.count() == 9);
    cell1 = cell2 = -1;
    for (cell1 = 0; cell1 < 9; cell1++)
        if (groupPossibilities[cell1].count() == 2)
            for (cell2 = cell1 + 1; cell2 < 9; cell2++)
                if (groupPossibilities[cell2].count() == 2)
                    if (groupPossibilities[cell1][0] == groupPossibilities[cell2][0] && groupPossibilities[cell1][1] == groupPossibilities[cell2][1])
                        return true;
    return false;
}

bool BoardModel::reduceCellGroupPossibilitiesForIdenticalPairs(CellGroupIteratorDirection direction, int param)
{
    // if within a "group" we find 2 cells
    // where each cell has just 2 possibilities *and* those numbers are the same in both cells
    // we can go through all *other* cells in the group removing those 2 numbers from their possibles
    QVector<QList<int> > groupPossibilities(cellGroupPossibilitiesByIndex(direction, param));

    int cell1, cell2;
    if (!findCellGroupIndexesForIdenticalPairs(groupPossibilities, cell1, cell2))
        return false;
    Q_ASSERT(cell1 >= 0 && cell1 < 9 && cell2 >= 0 && cell2 < 9 && cell1 != cell2);
    Q_ASSERT(groupPossibilities[cell1].count() == 2 && groupPossibilities[cell2].count() == 2);
    Q_ASSERT(groupPossibilities[cell1][0] == groupPossibilities[cell2][0] && groupPossibilities[cell1][1] == groupPossibilities[cell2][1]);
    int num1 = groupPossibilities[cell1][0], num2 = groupPossibilities[cell1][1];
    Q_ASSERT(num1 != num2);

    bool changed = false;
    for (CellGroupIterator cgit(direction, param); !cgit.atEnd(); cgit.next())
        if (cgit.groupIndex != cell1 && cgit.groupIndex != cell2)
            if (possibilities[cgit.row][cgit.col][num1] || possibilities[cgit.row][cgit.col][num2])
            {
                changed = true;
                setPossibility(cgit.row, cgit.col, num1, false);
                setPossibility(cgit.row, cgit.col, num2, false);
            }
    return changed;
}

bool BoardModel::reduceAllGroupPossibilitiesForIdenticalPairs()
{
    // find if there is a "group" (row/column/square) of cells
    // where there are 2 cells which both have just 2 possibilities and those are the same possibilities
    // from that we can reduce the possibilities in other members of the group to eliminate those 2 possibilities
    bool changed = false;
    for (CellGroupIteratorDirection direction : {Column, Row, Square})
        for (int param = 0; param < 9; param++)
            if (reduceCellGroupPossibilitiesForIdenticalPairs(direction, param))
                changed = true;
    return changed;
}

QList<int> BoardModel::groupIndexPossibilitiesListForNumber(CellGroupIteratorDirection direction, int param, int num) const
{
    // return a list of which cell indexes within a group are possible for a number
    QList<int> indexes;
    for (CellGroupIterator cgit(direction, param); !cgit.atEnd(); cgit.next())
        if (numInCell(cgit.row, cgit.col) == 0)
            if (possibilities[cgit.row][cgit.col][num])
                indexes.append(cgit.groupIndex);
    return indexes;
}

QVector<QList<int> > BoardModel::cellGroupPossibilitiesByNumber(CellGroupIteratorDirection direction, int param) const
{
    // return an array indexed by possibility number
    // where each array element is a list of which "group" element indexes are possible for the number
    QVector<QList<int> > groupVec(10);
    for (int num = 1; num <= 9; num++)
        groupVec[num] = groupIndexPossibilitiesListForNumber(direction, param, num);
    return groupVec;
}

bool BoardModel::findNumsForUniquePairs(const QVector<QList<int> > &groupPossibilities, int &num1, int &num2) const
{
    // return whether, by looking through the array of lists of possible elements of a group for each number,
    // there are any 2 elements which have just 2 possible locations *and* those locations are the same in both elements
    // in that case, set `num1` & `num2` to the numbers of the identical pair
    Q_ASSERT(groupPossibilities.count() == 10);
    num1 = num2 = 0;
    for (num1 = 1; num1 <= 9; num1++)
        if (groupPossibilities[num1].count() == 2)
            for (num2 = num1 + 1; num2 <= 9; num2++)
                if (groupPossibilities[num2].count() == 2)
                    if (groupPossibilities[num1][0] == groupPossibilities[num2][0] && groupPossibilities[num1][1] == groupPossibilities[num2][1])
                        return true;
    return false;
}

bool BoardModel::reduceCellGroupPossibilitiesForUniquePairs(CellGroupIteratorDirection direction, int param)
{
    // if within a "group" we find 2 cells
    // which have among their (any number of) possibilities some 2 common possible numbers
    // where neither of those 2 numbers is in any *other* cells' possibilities
    // we can reduce the possibilities in those 2 cells to eliminate any *other* possibilities
    // *and* then we can reduce the possibilities in any *other* cells to remove the 2 numbers
    QVector<QList<int> > groupPossibilities(cellGroupPossibilitiesByNumber(direction, param));

    int num1, num2;
    if (!findNumsForUniquePairs(groupPossibilities, num1, num2))
        return false;
    Q_ASSERT(num1 >= 1 && num1 <= 9 && num2 >= 1 && num2 <= 9 && num1 != num2);
    Q_ASSERT(groupPossibilities[num1].count() == 2 && groupPossibilities[num2].count() == 2);
    Q_ASSERT(groupPossibilities[num1][0] == groupPossibilities[num2][0] && groupPossibilities[num1][1] == groupPossibilities[num2][1]);
    int cell1 = groupPossibilities[num1][0], cell2 = groupPossibilities[num1][1];
    Q_ASSERT(cell1 != cell2);

    bool changed = false;
    for (CellGroupIterator cgit(direction, param); !cgit.atEnd(); cgit.next())
        if (cgit.groupIndex == cell1 || cgit.groupIndex == cell2)
        {
            for (int num = 1; num <= 9; num++)
                if (num != num1 && num != num2)
                    if (possibilities[cgit.row][cgit.col][num])
                    {
                        changed = true;
                        setPossibility(cgit.row, cgit.col, num, false);
                    }
        }
        else
        {
            if (possibilities[cgit.row][cgit.col][num1] || possibilities[cgit.row][cgit.col][num2])
            {
                changed = true;
                setPossibility(cgit.row, cgit.col, num1, false);
                setPossibility(cgit.row, cgit.col, num2, false);
            }
        }
    return changed;
}

bool BoardModel::reduceAllGroupPossibilitiesForUniquePairs()
{
    // find if there is a "group" (row/column/square) of cells
    // where there are just 2 cells which both have among their (any number of) possibilities
    // some 2 numbers neither of which is in any *other* cells' possibilities
    // from that we can reduce the possibilities in those 2 cells to eliminate any *other* possibilities
    // *and* then we will be able to reduce the possibilities in other members of the group to eliminate those 2 possibilities
    // per `reduceAllGroupPossibilitiesForIdenticalPairs()`
    bool changed = false;
    for (CellGroupIteratorDirection direction : {Column, Row, Square})
        for (int param = 0; param < 9; param++)
            if (reduceCellGroupPossibilitiesForUniquePairs(direction, param))
                changed = true;
    return changed;
}

CellNum BoardModel::solveFindStepPass3()
{
    bool changed;
    do
    {
        changed = reduceAllGroupPossibilitiesForIdenticalPairs();
        if (!changed)
            changed = reduceAllGroupPossibilitiesForUniquePairs();
        if (changed)
        {
            CellNum cellnum = solveFindStepPass1();
            if (!cellnum.isEmpty())
                return cellnum;
            cellnum = solveFindStepPass2();
            if (!cellnum.isEmpty())
                return cellnum;
        }
    } while (changed);
    return CellNum();
}

CellNum BoardModel::solveFindStep()
{
    CellNum cellNum;
    cellNum = solveFindStepPass1();
    if (!cellNum.isEmpty())
        return cellNum;
    cellNum = solveFindStepPass2();
    if (!cellNum.isEmpty())
        return cellNum;
    cellNum = solveFindStepPass3();
    if (!cellNum.isEmpty())
        return cellNum;
    return CellNum();
}

CellNum BoardModel::solveStep()
{
    if (!possibilitiesInitialised)
        solveStart();
    CellNum cellNum = solveFindStep();
    if (cellNum.isEmpty())
        return cellNum;
    setData(index(cellNum.row, cellNum.col), cellNum.num);
    reduceAllPossibilities();
    return cellNum;
}

bool BoardModel::numIsPossible(int num, const QModelIndex &index) const
{
    Q_ASSERT(num >= 1 && num <= 9);
    Q_ASSERT(index.isValid());
    return possibilities[index.row()][index.column()][num];
}

/*slot*/ void BoardModel::modelDataEdited()
{
    resetAllPossibilities();
    checkForDuplicates();
}

/*virtual*/ QVariant BoardModel::data(const QModelIndex &index, int role /*= Qt::DisplayRole*/) const /*override*/
{
    switch (role)
    {
    case Qt::FontRole: {
        QFont font;
        font.setPointSize(18);
        return font;
    }
    case Qt::TextAlignmentRole:
        return Qt::AlignCenter;
    default: break;
    }
    return QStandardItemModel::data(index, role);
}

/*virtual*/ Qt::ItemFlags BoardModel::flags(const QModelIndex &index) const /*override*/
{
    Q_UNUSED(index);
    return (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
}

/*virtual*/ bool BoardModel::setData(const QModelIndex &index, const QVariant &value, int role /*= Qt::EditRole*/) /*override*/
{
    switch (role)
    {
    case Qt::EditRole: {
        QVariant value2;
        if (!value.isValid() || value.toString().trimmed().isEmpty())
            value2 = QVariant();
        else
        {
            bool ok;
            int num = value.toInt(&ok);
            if (!ok || num < 1 || num > 9)
                return false;
            value2 = QVariant(num);
        }
        return QStandardItemModel::setData(index, value2, role);
    }
    default: break;
    }
    return QStandardItemModel::setData(index, value, role);
}


////////// CLASS BoardView //////////

BoardView::BoardView(QWidget *parent /*= nullptr*/)
    : QTableView(parent)
{
    horizontalHeader()->hide();
    verticalHeader()->hide();
    horizontalHeader()->setDefaultSectionSize(80);
    verticalHeader()->setDefaultSectionSize(80);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    setFixedSize(9 * 80 + 4, 9 * 80 + 4);

    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::AnyKeyPressed);

    cellDelegate = new BoardCellDelegate;
    setItemDelegate(cellDelegate);
    connect(cellDelegate, &BoardCellDelegate::modelDataEdited, this, &BoardView::modelDataEdited);

    connect(&cellFlasher.timer, &QTimer::timeout, this, &BoardView::cellFlasherTimeout);
}

void BoardView::setShowPossibilities(bool show)
{
    cellDelegate->setShowPossibilities(show);
    update();
}

void BoardView::flashCell(const QModelIndex &index)
{
    if (cellFlasher.index.isValid())
    {
        cellFlasher.countdown = 0;
        cellFlasherTimeout();
    }
    if (!index.isValid())
        return;
    cellFlasher.index = index;
    cellFlasher.countdown = 8;
    cellFlasher.timer.start(500);
}

/*virtual*/ void BoardView::setModel(QAbstractItemModel *model) /*override*/
{
    BoardModel *boardModel = qobject_cast<BoardModel *>(model);
    if (model)
        Q_ASSERT(boardModel);
    cellDelegate->setBoardModel(boardModel);
    QTableView::setModel(boardModel);
    if (boardModel)
        connect(boardModel, &QAbstractItemModel::modelAboutToBeReset, [this]() { cellFlasher.countdown = 0; cellFlasherTimeout(); });
}

void BoardView::cellFlasherTimeout()
{
    if (cellFlasher.countdown > 0)
        cellFlasher.countdown--;
    if (cellFlasher.index.isValid())
    {
        bool showHide = ((cellFlasher.countdown & 1) == 0);
        QVariant colour = showHide ? QVariant() : QColor(0, 0, 0, 0);
        model()->setData(cellFlasher.index, colour, Qt::ForegroundRole);
    }
    if (cellFlasher.countdown == 0)
    {
        cellFlasher.timer.stop();
        cellFlasher.index = QModelIndex();
    }
}


////////// CLASS BoardCellDelegate //////////

BoardCellDelegate::BoardCellDelegate(QObject *parent /*= nullptr*/)
    : QStyledItemDelegate(parent)
{
    _showPossibilities = false;
    boardModel = nullptr;
}

bool BoardCellDelegate::showPossibilities() const
{
    return _showPossibilities;
}

void BoardCellDelegate::setShowPossibilities(bool show)
{
    _showPossibilities = show;
}

void BoardCellDelegate::setBoardModel(BoardModel *boardModel)
{
    this->boardModel = boardModel;
}

/*virtual*/ QWidget *BoardCellDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const /*override*/
{
    Q_UNUSED(option);
    QComboBox *combo = new QComboBox(parent);
    combo->addItem("");
    for (int num = 1; num <= 9; num++)
        combo->addItem(QString::number(num));
    QVariant value(index.data());
    QString str(value.toString());
    combo->setCurrentText(str);
    return combo;
}

/*virtual*/ void BoardCellDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const /*override*/
{
    painter->save();
    QPen pen;
    pen.setWidth(2);
    pen.setColor(Qt::black);
    painter->setPen(pen);
    if (index.column() % 3 == 0)
        painter->drawLine(option.rect.topLeft(), option.rect.bottomLeft());
    else if (index.column() % 3 == 2)
        painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
    if (index.row() % 3 == 0)
        painter->drawLine(option.rect.topLeft(), option.rect.topRight());
    else if (index.row() % 3 == 2)
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    painter->restore();

    QVariant data(index.data());
    if (!data.isValid())
    {
        if (!_showPossibilities)
            return;
        painter->save();
        int w = option.rect.width() / 3, h = option.rect.height() / 3;
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 3; col++)
            {
                QRect rect(option.rect.x() + col * w, option.rect.y() + row * h, w, h);
                rect.adjust(4, 4, -4, -4);
                int num = row * 3 + col + 1;
                if (!boardModel->numIsPossible(num, index))
                    painter->fillRect(rect, Qt::darkGray);
                else
                {
                    painter->setPen(Qt::lightGray);
                    painter->drawText(rect, Qt::AlignCenter, QString::number(num));
                }
            }
        painter->restore();
        return;
    }
    QStyledItemDelegate::paint(painter, option, index);
}

/*virtual*/ void BoardCellDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const /*override*/
{
    QStyledItemDelegate::setModelData(editor, model, index);
    emit modelDataEdited(index);
}

