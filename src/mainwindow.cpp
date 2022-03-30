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
    board->setData(board->index(1, 1), 9);
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

BoardModel::CellGroupIterator::CellGroupIterator(CellGroupIteratorDirection direction, int param)
{
    this->direction = direction;
    row0 = col0 = 0;
    switch (direction)
    {
    case Row: row0 = param; break;
    case Column: col0 = param; break;
    case Square:
        row0 = param / 3;
        col0 = param % 3;
        break;
    }
    row = row0;
    col = col0;
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
    int numHere = index(row, col).data().toInt();
    if (numHere != 0)
    {
        for (int num = 1; num <= 9; num++)
            setPossibility(row, col, num, false);
    }
    if (numHere != 0)
    {
        for (int row2 = 0; row2 < 9; row2++)
            setPossibility(row2, col, numHere, false);
        for (int col2 = 0; col2 < 9; col2++)
            setPossibility(row, col2, numHere, false);
        int square = (row / 3) * 3 + (col / 3);
        int row0 = square / 3 * 3, col0 = square % 3 * 3;
        for (int row2 = row0; row2 < row0 + 3; row2++)
            for (int col2 = col0; col2 < col0 + 3; col2++)
                setPossibility(row2, col2, numHere, false);
    }
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

bool BoardModel::isSolved() const
{
    for (int row = 0; row < rowCount(); row++)
        for (int col = 0; col < columnCount(); col++)
            if (index(row, col).data().toInt() == 0)
                return false;
    return true;
}

bool BoardModel::numInCellHasDuplicate(int row, int col) const
{
    int num = index(row, col).data().toInt();
    if (num == 0)
        return false;
    for (int row2 = 0; row2 < 9; row2++)
        if (row2 != row && index(row2, col).data().toInt() == num)
                return true;
    for (int col2 = 0; col2 < 9; col2++)
        if (col2 != col && index(row, col2).data().toInt() == num)
                return true;
    int square = (row / 3) * 3 + (col / 3);
    int row0 = square / 3 * 3, col0 = square % 3 * 3;
    for (int row2 = row0; row2 < row0 + 3; row2++)
        for (int col2 = col0; col2 < col0 + 3; col2++)
            if (row2 != row && col2 != col && index(row2, col2).data().toInt() == num)
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
        {
            int num = data(index(row, col)).toInt();
            ts << num << " ";
        }
        ts << endl;
    }
}

void BoardModel::solveStart()
{
    resetAllPossibilities();
    reduceAllPossibilities();
}

bool BoardModel::solveCellHasOnePossibility(int row, int col, int &num) const
{
    num = 0;
    if (index(row, col).data().toInt() != 0)
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
    for (int row = 0; row < 9; row++)
        for (int col = 0; col < 9; col++)
        {
            int num = index(row, col).data().toInt();
            if (num != 0)
                continue;
            if (solveCellHasOnePossibility(row, col, num))
                return CellNum(row, col, num);
        }
    return CellNum();
}

CellNum BoardModel::cellGroupOnlyPossibilityForNum(int row, int col, int square) const
{
    if (row >= 0)
    {
        for (int col1 = 0; col1 < 9; col1++)
            if (index(row, col1).data().toInt() == 0)
                for (int num = 1; num <= 9; num++)
                    if (possibilities[row][col1][num])
                    {
                        int found = 0;
                        for (int col2 = 0; col2 < 9; col2++)
                            if (index(row, col2).data().toInt() == 0 && possibilities[row][col2][num])
                                found++;
                        if (found == 1)
                            return CellNum(row, col1, num);
                    }
    }
    else if (col >= 0)
    {
        for (int row1 = 0; row1 < 9; row1++)
            if (index(row1, col).data().toInt() == 0)
                for (int num = 1; num <= 9; num++)
                    if (possibilities[row1][col][num])
                    {
                        int found = 0;
                        for (int row2 = 0; row2 < 9; row2++)
                            if (index(row2, col).data().toInt() == 0 && possibilities[row2][col][num])
                                found++;
                        if (found == 1)
                            return CellNum(row1, col, num);
                    }
    }
    else if (square >= 0)
    {
        int row0 = square / 3 * 3, col0 = square % 3 * 3;
        for (int row1 = row0; row1 < row0 + 3; row1++)
            for (int col1 = col0; col1 < col0 + 3; col1++)
                if (index(row1, col).data().toInt() == 0)
                    for (int num = 1; num <= 9; num++)
                        if (possibilities[row1][col1][num])
                        {
                            int found = 0;
                            for (int row2 = row0; row2 < row0 + 3; row2++)
                                for (int col2 = col0; col2 < col0 + 3; col2++)
                                    if (index(row2, col2).data().toInt() == 0 && possibilities[row2][col2][num])
                                        found++;
                            if (found == 1)
                                return CellNum(row1, col1, num);
                        }
    }
    else
        Q_ASSERT(false);
    return CellNum();
}

CellNum BoardModel::solveFindStepPass2() const
{
    //VERYTEMPORARY
    return CellNum();
    // find if there is a "group" (row/column/square) of cells
    // where there is some possibility which is only available *once* in the group
    CellNum cellNum;
    for (int row = 0; row < 9; row++)
        if (!(cellNum = cellGroupOnlyPossibilityForNum(row, -1, -1)).isEmpty())
            return cellNum;
    for (int col = 0; col < 9; col++)
        if (!(cellNum = cellGroupOnlyPossibilityForNum(-1, col, -1)).isEmpty())
            return cellNum;
    for (int square = 0; square < 9; square++)
        if (!(cellNum = cellGroupOnlyPossibilityForNum(-1, -1, square)).isEmpty())
            return cellNum;
    return CellNum();
}

QList<int> BoardModel::numPossibilitiesList(int row, int col) const
{
    QList<int> nums;
    if (index(row, col).data().toInt() != 0)
        return nums;
    for (int num = 1; num <= 9; num++)
        if (possibilities[row][col][num])
            nums.append(num);
    return nums;
}

QVector<QList<int> > BoardModel::solveCellGroupPossibilities(int row, int col, int square) const
{
    QVector<QList<int> > groupVec(9);
    if (row >= 0)
    {
        for (int col1 = 0; col1 < 9; col1++)
            groupVec[col1] = numPossibilitiesList(row, col1);
    }
    else if (col >= 0)
    {
        for (int row1 = 0; row1 < 9; row1++)
            groupVec[row1] = numPossibilitiesList(row1, col);
    }
    else if (square >= 0)
    {
        int row0 = square / 3 * 3, col0 = square % 3 * 3;
        for (int i = 0, row1 = row0; row1 < row0 + 3; row1++)
            for (int col1 = col0; col1 < col0 + 3; col1++, i++)
                groupVec[i] = numPossibilitiesList(row1, col1);
    }
    else
        Q_ASSERT(false);
    return groupVec;
}

bool BoardModel::findCellGroupIndexesForIdenticalPairs(const QVector<QList<int> > &groupPossibilities, int &cell1, int &cell2) const
{
    cell1 = cell2 = -1;
    for (cell1 = 0; cell1 < 9; cell1++)
        if (groupPossibilities[cell1].count() == 2)
            for (cell2 = cell1 + 1; cell2 < 9; cell2++)
                if (groupPossibilities[cell2].count() == 2)
                    if (groupPossibilities[cell1][0] == groupPossibilities[cell2][0] && groupPossibilities[cell1][1] == groupPossibilities[cell2][1])
                        return true;
    return false;
}

bool BoardModel::reduceCellGroupPossibilitiesForIdenticalPairs(int row, int col, int square)
{
    QVector<QList<int> > groupPossibilities(solveCellGroupPossibilities(row, col, square));
    int cell1, cell2;
    if (!findCellGroupIndexesForIdenticalPairs(groupPossibilities, cell1, cell2))
        return false;
    Q_ASSERT(cell1 >= 0 && cell1 < 9 && cell2 >= 0 && cell2 < 9 && cell1 != cell2);
    Q_ASSERT(groupPossibilities[cell1].count() == 2 && groupPossibilities[cell2].count() == 2);
    int num1 = groupPossibilities[cell1][0], num2 = groupPossibilities[cell1][1];
    Q_ASSERT(num1 != num2);
    bool changed = false;
    if (row >= 0)
    {
        for (CellGroupIterator cgit(Row, row); !cgit.atEnd(); cgit.next())
//        for (int col1 = 0; col1 < 9; col1++)
            if (cgit.col != cell1 && cgit.col != cell2)
            {
                if (possibilities[cgit.row][cgit.col][num1] || possibilities[cgit.row][cgit.col][num2])
                    changed = true;
                setPossibility(cgit.row, cgit.col, num1, false);
                setPossibility(cgit.row, cgit.col, num2, false);
            }
    }
    else if (col >= 0)
    {
        for (int row1 = 0; row1 < 9; row1++)
            if (row1 != cell1 && row1 != cell2)
            {
                if (possibilities[row1][col][num1] || possibilities[row1][col][num2])
                    changed = true;
                setPossibility(row1, col, num1, false);
                setPossibility(row1, col, num2, false);
            }
    }
    else if (square >= 0)
    {
        int row0 = square / 3 * 3, col0 = square % 3 * 3;
        for (int i = 0, row1 = row0; row1 < row0 + 3; row1++)
            for (int col1 = col0; col1 < col0 + 3; col1++, i++)
                if (i != cell1 && i != cell2)
                {
                    if (possibilities[row1][col1][num1] || possibilities[row1][col1][num2])
                        changed = true;
                    setPossibility(row1, col1, num1, false);
                    setPossibility(row1, col1, num2, false);
                }
    }
    else
        Q_ASSERT(false);
    return changed;
}

CellNum BoardModel::solveFindStepPass3()
{
    // find if there is a "group" (row/column/square) of cells
    // where there are 2 cells which both have just 2 possibilities and those are the same possibilities
    // from that we can reduce the possibilities in other members of the group to eliminate those 2 possibilities
    bool changed;
    do
    {
        changed = false;
        for (int row = 0; row < 9; row++)
            if (reduceCellGroupPossibilitiesForIdenticalPairs(row, -1, -1))
                changed = true;
        for (int col = 0; col < 9; col++)
            if (reduceCellGroupPossibilitiesForIdenticalPairs(-1, col, -1))
                changed = true;
        for (int square = 0; square < 9; square++)
            if (reduceCellGroupPossibilitiesForIdenticalPairs(-1, -1, square))
                changed = true;
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

