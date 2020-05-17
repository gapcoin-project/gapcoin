#include "miningpage.h"
#include "ui_miningpage.h"
#include "init.h"
#include "main.h"
#include "miner.h"
#include "util.h"
#include "bitcoingui.h"
#include "rpcserver.h"
#include "walletmodel.h"
#include "clientmodel.h"

#include <boost/thread.hpp>
#include <boost/config.hpp>
#include <stdio.h>
#include <memory>
#include <cmath>

#include <QDebug>
#include <QValidator>
#include <QIntValidator>

extern json_spirit::Value GetNetworkHashPS(int lookup, int height);

static QString formatTimeInterval(CBigNum t)
{
    enum  EUnit { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, NUM_UNITS };

    const int SecondsPerUnit[NUM_UNITS] =
    {
        31556952, // average number of seconds in gregorian year
        31556952/12, // average number of seconds in gregorian month
        24*60*60, // number of seconds in a day
        60*60, // number of seconds in an hour
        60, // number of seconds in a minute
        1
    };

    const char* UnitNames[NUM_UNITS] =
    {
        "year",
        "month",
        "day",
        "hour",
        "minute",
        "second"
    };

    if (t > 0xFFFFFFFF)
    {
        t /= SecondsPerUnit[YEAR];
        return QString("%1 years").arg(t.ToString(10).c_str());
    }
    else
    {
        unsigned int t32 = t.getuint();

        int Values[NUM_UNITS];
        for (int i = 0; i < NUM_UNITS; i++)
        {
            Values[i] = t32/SecondsPerUnit[i];
            t32 %= SecondsPerUnit[i];
        }

        int FirstNonZero = 0;
        while (FirstNonZero < NUM_UNITS && Values[FirstNonZero] == 0)
            FirstNonZero++;

        QString TimeStr;
        for (int i = FirstNonZero; i < std::min(FirstNonZero + 3, (int)NUM_UNITS); i++)
        {
            int Value = Values[i];
            TimeStr += QString("%1 %2%3 ").arg(Value).arg(UnitNames[i]).arg((Value == 1)? "" : "s"); // FIXME: this is English specific
        }
        return TimeStr;
    }
}

static QString formatHashrate(int64_t n)
{
    if (n == 0)
        return "0 Primes/s";

    int i = (int)floor(log(n)/log(1000));
    float v = n*pow(1000.0f, -i);

    QString prefix = "";
    if (i >= 1 && i < 9)
        prefix = " kMGTPEZY"[i];

    return QString("%1 %2Primes/s").arg(v, 0, 'f', 2).arg(prefix);
}

MiningPage::MiningPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MiningPage),
    hasMiningprivkey(false)
{
    ui->setupUi(this);

    nThreads = boost::thread::hardware_concurrency();
    maxGenProc = GetArg("-genproclimit", -1);
    if (maxGenProc < 0)
         nUseThreads = nThreads;
    else
        nUseThreads = maxGenProc;

    ui->sliderCores->setMinimum(0);
    ui->sliderCores->setMaximum(nThreads);
    ui->sliderCores->setValue(nUseThreads);
    ui->labelNCores->setText(QString("%1").arg(nUseThreads));

    isMining = GetBoolArg("-gen", false)? 1 : 0;
    connect(ui->sliderCores, SIGNAL(valueChanged(int)), this, SLOT(changeNumberOfCores(int)));
    connect(ui->pushSwitchMining, SIGNAL(clicked()), this, SLOT(switchMining()));

    QValidator *headershiftValuevalidator = new QIntValidator(14, 512, this);
    ui->headershiftValue->setValidator(headershiftValuevalidator);
    QValidator *sievesizeValuevalidator = new QIntValidator(1000, 33554432, this);
    ui->sievesizeValue->setValidator(sievesizeValuevalidator);
    QValidator *sieveprimesValuevalidator = new QIntValidator(1000, 900000, this);
    ui->sieveprimesValue->setValidator(sieveprimesValuevalidator);

    // setup Plot
    // create graph
    ui->diffplot_difficulty->addGraph();

    // Use usual background
    ui->diffplot_difficulty->setBackground(QBrush(QColor(255,255,255,255)));//QWidget::palette().color(this->backgroundRole())));

    // give the axes some labels:
    ui->diffplot_difficulty->xAxis->setLabel(tr("Block height"));
    ui->diffplot_difficulty->yAxis->setLabel(tr("Difficulty"));

    // set the pens
    //a13469
    // ui->diffplot_difficulty->graph(0)->setPen(QPen(QColor(255, 165, 18), 3, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));//QPen(QColor(76, 76, 229)));
    // ui->diffplot_difficulty->graph(0)->setLineStyle(QCPGraph::lsLine);

    // set the pens
    ui->diffplot_difficulty->graph(0)->setPen(QPen(QColor(5, 168, 162)));
    ui->diffplot_difficulty->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->diffplot_difficulty->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(5, 168, 162), 1));

    // set axes label fonts:
    QFont label = font();
    ui->diffplot_difficulty->xAxis->setLabelFont(label);
    ui->diffplot_difficulty->xAxis->setTickLabelRotation(15);
    ui->diffplot_difficulty->yAxis->setLabelFont(label);
    ui->diffplot_difficulty->xAxis->setAutoSubTicks(false);
    ui->diffplot_difficulty->yAxis->setAutoSubTicks(false);
    ui->diffplot_difficulty->xAxis->setSubTickCount(4);
    ui->diffplot_difficulty->xAxis->setNumberFormat("f");
    ui->diffplot_difficulty->xAxis->setNumberPrecision(0);
    ui->diffplot_difficulty->yAxis->setSubTickCount(0);


    // setup Plot
    // create graph
    ui->diffplot_hashrate->addGraph();

    // Use usual background
    ui->diffplot_hashrate->setBackground(QBrush(QColor(255,255,255,255)));//QWidget::palette().color(this->backgroundRole())));

    // give the axes some labels:
    ui->diffplot_hashrate->xAxis->setLabel(tr("Block height"));
    ui->diffplot_hashrate->yAxis->setLabel(tr("Hashrate MPrimes/s"));

    // set the pens
    //a13469, 6c3d94
    // ui->diffplot_hashrate->graph(0)->setPen(QPen(QColor(255, 165, 18), 3, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));//QPen(QColor(76, 76, 229)));
    // ui->diffplot_hashrate->graph(0)->setLineStyle(QCPGraph::lsLine);

    // set the pens
    ui->diffplot_hashrate->graph(0)->setPen(QPen(QColor(5, 168, 162)));
    ui->diffplot_hashrate->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->diffplot_hashrate->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(5, 168, 162), 1));

    // set axes label fonts:
    QFont label2 = font();
    ui->diffplot_hashrate->xAxis->setLabelFont(label2);
    ui->diffplot_hashrate->yAxis->setLabelFont(label2);
    ui->diffplot_hashrate->xAxis->setTickLabelRotation(15);
    ui->diffplot_hashrate->xAxis->setAutoSubTicks(false);
    ui->diffplot_hashrate->yAxis->setAutoSubTicks(false);
    ui->diffplot_hashrate->xAxis->setSubTickCount(4);
    ui->diffplot_hashrate->xAxis->setNumberFormat("f");
    ui->diffplot_hashrate->xAxis->setNumberPrecision(0);
    ui->diffplot_hashrate->yAxis->setSubTickCount(0);

    updateUI(isMining);
    startTimer(8000);
}

MiningPage::~MiningPage()
{
    delete ui;
}

void MiningPage::setModel(WalletModel *model)
{
    this->model = model;
}

void MiningPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
}

void MiningPage::updateUI(bool fGenerate)
{
    ui->labelNCores->setText(QString("%1").arg(nUseThreads));
    ui->sliderCores->setValue(nUseThreads);
    ui->pushSwitchMining->setText(fGenerate? tr("Stop mining") : tr("Start mining"));

    CBlockIndex* pindexBest = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
    int64_t Hashrate = (int64_t)dHashesPerSec;
    int64_t NetworkHashrate = GetNetworkHashPS(120, -1).get_int64();
    ui->labelYourHashrate->setText(formatHashrate(Hashrate));
    ui->labelNethashrate->setText(formatHashrate(NetworkHashrate));
    // clientModel->setMining(fGenerate, dHashesPerSec);

    QString NextBlockTime;
    if (Hashrate == 0)
        NextBlockTime = QChar(L'∞');
    else
    {
        CBigNum Target;
        Target.SetCompact(pindexBest->nDifficulty);
        CBigNum ExpectedTime = (CBigNum(1) << 256)/(Target*Hashrate);
        NextBlockTime = formatTimeInterval(ExpectedTime);
    }
    ui->labelNextBlock->setText(NextBlockTime);

    QString status = QString("Not Mining Gapcoin");
    if (fGenerate)
        status = QString("Mining with %1 threads, shift: %2, sieve size: %3, number of primes in sieve: %4 - hashrate: %5 (%6 tests per sec.)")
            .arg(nUseThreads).arg(nMiningShift).arg(nMiningSieveSize).arg(nMiningPrimes).arg(formatHashrate(Hashrate)).arg(dTestsPerSec);
    ui->miningStatistics->setText(status);
}

void MiningPage::restartMining(bool fGenerate, int nThreads)
{
    isMining = fGenerate;
    if (nThreads <= maxGenProc)
        nUseThreads = nThreads;

    // unlock wallet before mining

  #ifndef __linux__
    if (fGenerate && !hasMiningprivkey && !unlockContext.get())
    {
        this->unlockContext.reset(new WalletModel::UnlockContext(model->requestUnlock()));
        if (!unlockContext->isValid())
        {
            unlockContext.reset(nullptr);
            return;
        }
    }
  #endif

    nMiningShift = ui->headershiftValue->text().toInt();
    nMiningSieveSize = ui->sievesizeValue->text().toInt();
    nMiningPrimes = ui->sieveprimesValue->text().toInt();
    clientModel->setMining(fGenerate, dHashesPerSec);
    GenerateGapcoins(fGenerate, pwalletMain, nUseThreads);

    // lock wallet after mining
    if (!fGenerate && !hasMiningprivkey)
        unlockContext.reset(nullptr);

    updateUI(fGenerate);
}

void MiningPage::changeNumberOfCores(int i)
{
    restartMining(isMining, i);
}

void MiningPage::switchMining()
{
    restartMining(!isMining, ui->sliderCores->value());
}

void MiningPage::updateSievePrimes(int i)
{

    sievesizeValue = pow(2, i);
    qDebug() << "New header shift: " << QString("%1").arg(i) << ", new sieve size value: " << QString("%1").arg(sievesizeValue);
    ui->sievesizeValue->setText(QString("%1").arg(sievesizeValue));
}

void MiningPage::updatePlot(int count)
{
    static int64_t lastUpdate = 0;
    CBlockIndex* pindexBest = mapBlockIndex[chainActive.Tip()->GetBlockHash()];

    // Double Check to make sure we don't try to update the plot when it is disabled
    if(!GetBoolArg("-chart", true)) { return; }
    if (GetTime() - lastUpdate < 60) { return; } // This is just so it doesn't redraw rapidly during syncing

    int numLookBack = 4032;
    double diffMax = 0;
    CBlockIndex* pindex = pindexBest;
    // int nBestHeight = pindexBest->nHeight;
    int height = pindexBest->nHeight;
    int xStart = std::max<int>(height-numLookBack, 0) + 1;
    int xEnd = height;

    // Start at the end and walk backwards
    int i = numLookBack-1;
    int x = xEnd;

    // This should be a noop if the size is already 2000
    vX.resize(numLookBack);
    vY.resize(numLookBack);

    CBlockIndex* itr = pindex;

    while(i >= 0 && itr != NULL)
    {
        vX[i] = itr->nHeight;
        vY[i] = GetDifficulty(itr);
        diffMax = std::max<double>(diffMax, vY[i]);

        itr = itr->pprev;
        i--;
        x--;
    }

    ui->diffplot_difficulty->graph(0)->setData(vX, vY);

    // set axes ranges, so we see all data:
    ui->diffplot_difficulty->xAxis->setRange((double)xStart, (double)xEnd);
    ui->diffplot_difficulty->yAxis->setRange(0, diffMax+(diffMax/10));

    ui->diffplot_difficulty->replot();

    //

    diffMax = 0;

    // Start at the end and walk backwards
    i = numLookBack-1;
    x = xEnd;

    // This should be a noop if the size is already 2000
    vX2.resize(numLookBack);
    vY2.resize(numLookBack);

    CBlockIndex* itr2 = pindex;

    while(i >= 0 && itr2 != NULL)
    {
        vX2[i] = itr2->nHeight;
        // vY2[i] =  (double)GetNetworkHashPS(120, itr2->nHeight).get_int64()/1000000;//GetDifficulty(itr);
        // qint64 NetworkHashrate = (qint64)GetNetworkHashPS(120, -1).get_int64();
        vY2[i] =  (double)GetNetworkHashPS(120, itr2->nHeight).get_int64();//GetDifficulty(itr);
        diffMax = std::max<double>(diffMax, vY2[i]);

        itr2 = itr2->pprev;
        i--;
        x--;
    }

    ui->diffplot_hashrate->graph(0)->setData(vX2, vY2);

    // set axes ranges, so we see all data:
    ui->diffplot_hashrate->xAxis->setRange((double)xStart, (double)xEnd);
    ui->diffplot_hashrate->yAxis->setRange(0, diffMax+(diffMax/10));

    ui->diffplot_hashrate->replot();

    lastUpdate = GetTime();
}

void MiningPage::timerEvent(QTimerEvent *)
{
    updateUI(isMining);
}
