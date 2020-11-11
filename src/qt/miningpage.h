#ifndef MININGPAGE_H
#define MININGPAGE_H

#include <QWidget>
#include <memory>
#if __STDC_VERSION__ < 201112L
#include <boost/move/unique_ptr.hpp>
#endif

#include "walletmodel.h"

namespace Ui {
class MiningPage;
}

class ClientModel;
class WalletModel;

class MiningPage : public QWidget
{
    Q_OBJECT

public:
    explicit MiningPage(QWidget *parent = 0);
    ~MiningPage();

    void setModel(WalletModel *model);
    void setClientModel(ClientModel *model);
    void updatePlot(int count);

private:
    Ui::MiningPage *ui;
    ClientModel *clientModel;
    WalletModel *model;
    int maxGenProc;
    int nThreads;
    int nUseThreads;
#if __STDC_VERSION__ < 201112L
    boost::movelib::unique_ptr<WalletModel::UnlockContext> unlockContext;
#else
    std::unique_ptr<WalletModel::UnlockContext> unlockContext;
#endif
    bool hasMiningprivkey;

    QVector<double> vX;
    QVector<double> vY;

    QVector<double> vX2;
    QVector<double> vY2;

    void restartMining(bool fGenerate, int nThreads);
    void timerEvent(QTimerEvent *event);
    void updateUI(bool fGenerate);
    int shiftValue;
    long long int sievesizeValue;
    int sieveprimesValue;
    bool isMining;

private Q_SLOTS:

    void changeNumberOfCores(int i);
    void switchMining();
    void updateSievePrimes(int i);
};

#endif // MININGPAGE_H
