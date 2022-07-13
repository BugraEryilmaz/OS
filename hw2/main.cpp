#include "hw2_output.h"
#include <errno.h>
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <tuple>
#include <vector>

using namespace std;

enum ORDER { NOORDER, BREAK, CONTINUE, STOP };

pthread_mutex_t grid_Lock = PTHREAD_MUTEX_INITIALIZER;
vector<vector<int> > grid;

pthread_mutex_t grid_ReaderWriter_Lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t grid_ReaderWriter_cond = PTHREAD_COND_INITIALIZER;
vector<vector<int> > grid_SmokingArea;
vector<vector<int> > grid_Smokers;
vector<vector<int> > grid_Collectors;
pthread_cond_t WaitGatherCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t WaitOrderCond = PTHREAD_COND_INITIALIZER;

int sleepingCollector = 0;
pthread_mutex_t sleepingCollector_Lock = PTHREAD_MUTEX_INITIALIZER;

int activeCollector = 0;
pthread_mutex_t activeCollector_Lock = PTHREAD_MUTEX_INITIALIZER;

int activeSmoker = 0;
pthread_mutex_t activeSmoker_Lock = PTHREAD_MUTEX_INITIALIZER;

ORDER order = CONTINUE;
int order_received = 0;
pthread_mutex_t order_received_Lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t waitOrderCond = PTHREAD_COND_INITIALIZER;

template <typename T> void* threadStarter(void* arg);

tuple<int, int> lookup[] = {make_tuple(0, 0), make_tuple(0, 1), make_tuple(0, 2), make_tuple(1, 2),
                            make_tuple(2, 2), make_tuple(2, 1), make_tuple(2, 0), make_tuple(1, 0)};

class Smokers {

private:
    int id, sleepDuration;
    vector<tuple<int, int, int> > locations;
    pthread_t threadId;
    const int sizex = 3, sizey = 3;

public:
    Smokers(int id, int sleepDuration, vector<tuple<int, int, int> > locations)
        : id(id), sleepDuration(sleepDuration), locations(locations) {}
    ~Smokers() {
        const int err = pthread_join(threadId, NULL);
        if (err) {
            cerr << "Thread join failed" << err;
            exit(5);
        }
    }

    void startThread() {
        int err = pthread_create(&threadId, NULL, &(threadStarter<Smokers>), this);
        if (err) {
            cerr << "Thread creation failed" << err;
            exit(5);
        }
    }

    void unlockGrid(tuple<int, int, int>& loc) {
        int x = get<0>(loc), y = get<1>(loc);
        pthread_mutex_lock(&grid_ReaderWriter_Lock);
        for (int i = 0; i < sizex; i++) {
            for (int j = 0; j < sizey; j++) {
                grid_SmokingArea[x + i][y + j] -= 1;
            }
        }
        pthread_mutex_unlock(&grid_ReaderWriter_Lock);
        grid_Smokers[x + 1][y + 1] = 0;
        pthread_cond_broadcast(&grid_ReaderWriter_cond);
    }

    void lockGrid(tuple<int, int, int>& loc) {
        // Try to lock all areas
        int x = get<0>(loc), y = get<1>(loc);
        pthread_mutex_lock(&grid_ReaderWriter_Lock);

        for (int i = 0; i < sizex; i++) {
            for (int j = 0; j < sizey; j++) {
                if (order == STOP) {
                    pthread_mutex_unlock(&grid_ReaderWriter_Lock);
                    hw2_notify(SNEAKY_SMOKER_STOPPED, id, 0, 0);
                    pthread_mutex_lock(&order_received_Lock);
                    order_received++;
                    pthread_cond_signal(&waitOrderCond);
                    pthread_mutex_unlock(&order_received_Lock);
                    pthread_mutex_lock(&activeSmoker_Lock);
                    activeSmoker--;
                    pthread_mutex_unlock(&activeSmoker_Lock);
                    pthread_exit(0);
                }
                if (grid_Collectors[x + i][y + j] || grid_Smokers[x + 1][y + 1]) {
                    // there is a reader writer wait until a signal
                    pthread_cond_wait(&grid_ReaderWriter_cond, &grid_ReaderWriter_Lock);

                    // restart the i
                    i = -1;
                    break;
                }
            }
        }
        // we can capture all locks so lock them
        for (int i = 0; i < sizex; i++) {
            for (int j = 0; j < sizey; j++) {
                grid_SmokingArea[x + i][y + j] += 1;
            }
        }
        grid_Smokers[x + 1][y + 1] = 1;
        pthread_mutex_unlock(&grid_ReaderWriter_Lock);

        if (order == STOP) {
            hw2_notify(SNEAKY_SMOKER_STOPPED, id, 0, 0);
            pthread_mutex_lock(&order_received_Lock);

            order_received++;
            pthread_cond_signal(&waitOrderCond);

            pthread_mutex_unlock(&order_received_Lock);
            pthread_mutex_lock(&activeSmoker_Lock);
            activeSmoker--;
            pthread_mutex_unlock(&activeSmoker_Lock);
            unlockGrid(loc);
            pthread_exit(0);
        }
        hw2_notify(SNEAKY_SMOKER_ARRIVED, id, x + 1, y + 1);
    }

    void setTimer(timespec& to) {
        int secs = sleepDuration / 1000;
        int nsecs = (sleepDuration % 1000) * 1000000;
        const int gettime_rv = clock_gettime(CLOCK_REALTIME, &to);
        if (gettime_rv) {
            cerr << "clock gettime error";
            exit(3);
        }
        to.tv_sec += secs + ((to.tv_nsec + nsecs) / 1000000000);
        to.tv_nsec = (to.tv_nsec + nsecs) % 1000000000;
    }

    void timedWait(tuple<int, int, int>& loc) {
        pthread_mutex_t WaitLock = PTHREAD_MUTEX_INITIALIZER;

        timespec to = {0, 0};

        pthread_mutex_lock(&WaitLock);
        setTimer(to);
        int ret = pthread_cond_timedwait(&WaitGatherCond, &WaitLock, &to);
        while (ret == 0) {
            // order came
            if (order == STOP) {
                pthread_mutex_unlock(&WaitLock);
                hw2_notify(SNEAKY_SMOKER_STOPPED, id, 0, 0);
                pthread_mutex_lock(&order_received_Lock);

                order_received++;
                pthread_cond_signal(&waitOrderCond);

                pthread_mutex_unlock(&order_received_Lock);
                pthread_mutex_lock(&activeCollector_Lock);
                activeCollector--;
                pthread_mutex_unlock(&activeCollector_Lock);
                unlockGrid(loc);
                pthread_exit(0);
            } else {
                ret = pthread_cond_timedwait(&WaitGatherCond, &WaitLock, &to);
            }
        }
        pthread_mutex_unlock(&WaitLock);
        if (ret == ETIMEDOUT) {
            // timeout
            return;
        } else {
            cerr << ret << " Conditional Wait Error";
            exit(4);
        }
    }

    void litterCells(tuple<int, int, int>& loc) {
        int x = get<0>(loc), y = get<1>(loc), litterCount = get<2>(loc);
        bool waited = false;
        for (int i = 0; i < litterCount; i++) {
            // after waiting litter the cig butt
            int litterRound = i / 8;
            int litterIndex = i % 8;
            int litterx = get<0>(lookup[litterIndex]);
            int littery = get<1>(lookup[litterIndex]);
            timedWait(loc);
            hw2_notify(SNEAKY_SMOKER_FLICKED, id, x + litterx, y + littery);
            pthread_mutex_lock(&grid_Lock);
            grid[x + litterx][y + littery]++;
            pthread_mutex_unlock(&grid_Lock);
        }

        hw2_notify(SNEAKY_SMOKER_LEFT, id, 0, 0);
    }

    void start() {
        hw2_notify(SNEAKY_SMOKER_CREATED, id, 0, 0);
        pthread_mutex_lock(&activeSmoker_Lock);
        activeSmoker++;
        pthread_mutex_unlock(&activeSmoker_Lock);

        for (auto loc : locations) {
            lockGrid(loc);
            litterCells(loc);
            unlockGrid(loc);
        }
        hw2_notify(SNEAKY_SMOKER_EXITED, id, 0, 0);
        pthread_mutex_lock(&activeSmoker_Lock);
        activeSmoker--;
        pthread_mutex_unlock(&activeSmoker_Lock);
    }
};

class Cleaners {

private:
    int id, sizex, sizey, sleepDuration;
    vector<tuple<int, int> > locations;
    pthread_t threadId;

public:
    Cleaners(int id, int sizex, int sizey, int sleepDuration, vector<tuple<int, int> > locations)
        : id(id), sizex(sizex), sizey(sizey), sleepDuration(sleepDuration), locations(locations) {}
    ~Cleaners() {
        const int err = pthread_join(threadId, NULL);
        if (err) {
            cerr << "Thread join failed" << err;
            exit(5);
        }
    }

    void startThread() {
        int err = pthread_create(&threadId, NULL, &(threadStarter<Cleaners>), this);
        if (err) {
            cerr << "Thread creation failed" << err;
            exit(5);
        }
    }

    void unlockGrid(tuple<int, int>& loc) {
        for (int i = 0; i < sizex; i++) {
            for (int j = 0; j < sizey; j++) {
                grid_Collectors[get<0>(loc) + i][get<1>(loc) + j] = 0;
            }
        }
        pthread_cond_broadcast(&grid_ReaderWriter_cond);
    }

    void lockGrid(tuple<int, int>& loc) {
        // Try to lock all areas
        pthread_mutex_lock(&grid_ReaderWriter_Lock);
        int x = get<0>(loc), y = get<1>(loc);

        for (int i = 0; i < sizex; i++) {
            for (int j = 0; j < sizey; j++) {
                if (order == BREAK) {
                    pthread_mutex_unlock(&grid_ReaderWriter_Lock);
                    // take break already tries to lock the grid again as it returns
                    // so no need to continue this loop
                    return takeBreak(loc, false);
                } else if (order == STOP) {
                    pthread_mutex_unlock(&grid_ReaderWriter_Lock);
                    hw2_notify(PROPER_PRIVATE_STOPPED, id, 0, 0);
                    pthread_mutex_lock(&order_received_Lock);

                    order_received++;
                    pthread_cond_signal(&waitOrderCond);

                    pthread_mutex_unlock(&order_received_Lock);
                    pthread_mutex_lock(&activeCollector_Lock);
                    activeCollector--;
                    pthread_mutex_unlock(&activeCollector_Lock);
                    pthread_exit(0);
                }
                if (grid_SmokingArea[x + i][y + j] || grid_Collectors[x + i][y + j]) {
                    // there is a reader writer wait until a signal
                    pthread_cond_wait(&grid_ReaderWriter_cond, &grid_ReaderWriter_Lock);

                    // restart the i
                    i = -1;
                    break;
                }
            }
        }
        // we can capture all locks so lock them
        for (int i = 0; i < sizex; i++) {
            for (int j = 0; j < sizey; j++) {
                grid_Collectors[get<0>(loc) + i][get<1>(loc) + j] = 1;
            }
        }
        pthread_mutex_unlock(&grid_ReaderWriter_Lock);

        if (order == BREAK) {
            // take break already tries to lock the grid again as it returns
            // so no need to continue this loop
            return takeBreak(loc, true);
        } else if (order == STOP) {
            hw2_notify(PROPER_PRIVATE_STOPPED, id, 0, 0);
            pthread_mutex_lock(&order_received_Lock);

            order_received++;
            pthread_cond_signal(&waitOrderCond);

            pthread_mutex_unlock(&order_received_Lock);
            pthread_mutex_lock(&activeCollector_Lock);
            activeCollector--;
            pthread_mutex_unlock(&activeCollector_Lock);
            unlockGrid(loc);
            pthread_exit(0);
        }
        return hw2_notify(PROPER_PRIVATE_ARRIVED, id, get<0>(loc), get<1>(loc));
    }

    void setTimer(timespec& to) {
        int secs = sleepDuration / 1000;
        int nsecs = (sleepDuration % 1000) * 1000000;
        const int gettime_rv = clock_gettime(CLOCK_REALTIME, &to);
        if (gettime_rv) {
            cerr << "clock gettime error";
            exit(3);
        }
        to.tv_sec += secs + ((to.tv_nsec + nsecs) / 1000000000);
        to.tv_nsec = (to.tv_nsec + nsecs) % 1000000000;
    }

    int timedWait(tuple<int, int>& loc) {
        int tookBreak = 0;
        pthread_mutex_t WaitLock = PTHREAD_MUTEX_INITIALIZER;

        timespec to = {0, 0};

        pthread_mutex_lock(&WaitLock);
        // cerr << secs << "s" << nsecs << "ns"
        //      << " sleep duration\n";
        setTimer(to);
        int ret = pthread_cond_timedwait(&WaitGatherCond, &WaitLock, &to);
        while (ret == 0) {
            pthread_mutex_unlock(&WaitLock);
            // order came
            if (order == BREAK) {
                tookBreak = 1;
                takeBreak(loc, true);
                pthread_mutex_lock(&WaitLock);
                setTimer(to);
                ret = pthread_cond_timedwait(&WaitGatherCond, &WaitLock, &to);
            } else if (order == STOP) {
                hw2_notify(PROPER_PRIVATE_STOPPED, id, 0, 0);
                pthread_mutex_lock(&order_received_Lock);

                order_received++;
                pthread_cond_signal(&waitOrderCond);

                pthread_mutex_unlock(&order_received_Lock);
                pthread_mutex_lock(&activeCollector_Lock);
                activeCollector--;
                pthread_mutex_unlock(&activeCollector_Lock);
                unlockGrid(loc);
                pthread_exit(0);
            } else {
                pthread_mutex_lock(&WaitLock);
                ret = pthread_cond_timedwait(&WaitGatherCond, &WaitLock, &to);
            }
        }
        pthread_mutex_unlock(&WaitLock);
        if (ret == ETIMEDOUT) {
            // timeout
            return tookBreak;
        } else {
            cerr << ret << " Conditional Wait Error";
            exit(4);
        }
    }

    void cleanCells(tuple<int, int>& loc) {
        bool waited = false;
        int x = get<0>(loc), y = get<1>(loc);
        for (int i = 0; i < sizex; i++) {
            for (int j = 0; j < sizey;) {
                // check for the next cig butt to collect
                if (grid[x + i][y + j] == 0) {
                    // there is no cig butt here continue with next tile
                    j++;
                    continue;
                }
                if (!waited) {
                    int tookBreak = timedWait(loc);
                    waited = true;
                    if (tookBreak) {
                        // return back to the top left corner
                        i = -1;
                        break;
                    }
                }
                // collect the cig butt
                hw2_notify(PROPER_PRIVATE_GATHERED, id, x + i, y + j);
                grid[x + i][y + j]--;
                waited = false;
                // prepare for next wait
            }
        }
        hw2_notify(PROPER_PRIVATE_CLEARED, id, 0, 0);
    }

    void takeBreak(tuple<int, int>& loc, bool releaseLock = true) {
        hw2_notify(PROPER_PRIVATE_TOOK_BREAK, id, 0, 0);
        pthread_mutex_lock(&order_received_Lock);

        order_received++;
        pthread_cond_signal(&waitOrderCond);

        pthread_mutex_unlock(&order_received_Lock);
        pthread_mutex_lock(&sleepingCollector_Lock);
        sleepingCollector++;
        pthread_mutex_unlock(&sleepingCollector_Lock);
        if (releaseLock) {
            unlockGrid(loc);
        }
        pthread_mutex_t breakLock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&breakLock);
        while (pthread_cond_wait(&WaitOrderCond, &breakLock) == 0) {
            if (order == CONTINUE) {
                pthread_mutex_unlock(&breakLock);
                hw2_notify(PROPER_PRIVATE_CONTINUED, id, 0, 0);
                pthread_mutex_lock(&order_received_Lock);

                order_received++;
                pthread_cond_signal(&waitOrderCond);
                pthread_mutex_unlock(&order_received_Lock);
                pthread_mutex_lock(&sleepingCollector_Lock);
                sleepingCollector--;
                pthread_mutex_unlock(&sleepingCollector_Lock);
                lockGrid(loc);
                return;
            } else if (order == STOP) {
                pthread_mutex_unlock(&breakLock);
                hw2_notify(PROPER_PRIVATE_STOPPED, id, 0, 0);
                pthread_mutex_lock(&order_received_Lock);

                order_received++;
                pthread_cond_signal(&waitOrderCond);

                pthread_mutex_unlock(&order_received_Lock);
                pthread_mutex_lock(&sleepingCollector_Lock);
                sleepingCollector--;
                pthread_mutex_unlock(&sleepingCollector_Lock);
                pthread_mutex_lock(&activeCollector_Lock);
                activeCollector--;
                pthread_mutex_unlock(&activeCollector_Lock);
                pthread_exit(0);
            }
        }
        exit(-1);
    }

    void start() {
        hw2_notify(PROPER_PRIVATE_CREATED, id, 0, 0);
        pthread_mutex_lock(&activeCollector_Lock);
        activeCollector++;
        pthread_mutex_unlock(&activeCollector_Lock);

        for (auto loc : locations) {
            lockGrid(loc);
            cleanCells(loc);
            unlockGrid(loc);
        }
        hw2_notify(PROPER_PRIVATE_EXITED, id, 0, 0);
        pthread_mutex_lock(&activeCollector_Lock);
        activeCollector--;
        pthread_mutex_unlock(&activeCollector_Lock);
    }
};

class OrderGivers {
private:
    vector<tuple<ORDER, timespec> > orders_sleeps;
    pthread_t threadId;
    timespec initialTime;

public:
    OrderGivers(timespec initialTime, vector<tuple<ORDER, int> > orders_sleeps) : initialTime(initialTime) {
        this->orders_sleeps.reserve(orders_sleeps.size());
        for (auto& order_sleep : orders_sleeps) {
            timespec to;
            setTimer(to, get<1>(order_sleep));
            this->orders_sleeps.push_back(make_tuple(get<0>(order_sleep), to));
        }
        int err = pthread_create(&threadId, NULL, &(threadStarter<OrderGivers>), this);
        if (err) {
            cerr << "Thread creation failed" << err;
            exit(5);
        }
    }
    ~OrderGivers() {
        const int err = pthread_join(threadId, NULL);
        if (err) {
            cerr << "Thread join failed" << err;
            exit(5);
        }
    }
    void setTimer(timespec& to, int duration) {
        int secs = duration / 1000;
        int nsecs = (duration % 1000) * 1000000;
        to = initialTime;
        to.tv_sec += secs + ((to.tv_nsec + nsecs) / 1000000000);
        to.tv_nsec = (to.tv_nsec + nsecs) % 1000000000;
    }

    void timedWait(timespec& to) {
        pthread_mutex_t WaitLock = PTHREAD_MUTEX_INITIALIZER;

        pthread_mutex_lock(&WaitLock);
        int ret = pthread_cond_timedwait(&waitOrderCond, &WaitLock, &to);
        while (ret == 0) {
            // unintentional wake up
            ret = pthread_cond_timedwait(&waitOrderCond, &WaitLock, &to);
        }
        pthread_mutex_unlock(&WaitLock);
        if (ret == ETIMEDOUT) {
            // timeout
            return;
        } else {
            cerr << ret << " Conditional Wait Error";
            exit(4);
        }
    }

    void waitForOrderReceived(int requiredwait) {
        pthread_mutex_t WaitLock = PTHREAD_MUTEX_INITIALIZER;

        pthread_mutex_lock(&WaitLock);
        while (requiredwait != order_received) {
            pthread_cond_wait(&waitOrderCond, &WaitLock);
        }
        pthread_mutex_unlock(&WaitLock);
    }

    int giveOrder(ORDER neworder) {
        int requiredReceive;
        switch (neworder) {
        case BREAK:
            requiredReceive = activeCollector - sleepingCollector;
            hw2_notify(ORDER_BREAK, 0, 0, 0);
            break;
        case CONTINUE:
            requiredReceive = sleepingCollector;
            hw2_notify(ORDER_CONTINUE, 0, 0, 0);
            break;
        case STOP:
            requiredReceive = activeCollector + activeSmoker;
            hw2_notify(ORDER_STOP, 0, 0, 0);
            break;
        }
        order_received = 0;
        order = neworder;
        pthread_cond_broadcast(&WaitGatherCond);
        pthread_cond_broadcast(&WaitOrderCond);
        pthread_cond_broadcast(&grid_ReaderWriter_cond);
        return requiredReceive;
    }

    void start() {
        for (auto& order_sleep : orders_sleeps) {
            ORDER order = get<0>(order_sleep);
            timespec sleepDuration = get<1>(order_sleep);
            timedWait(sleepDuration);
            int requiredreceive = giveOrder(order);
            waitForOrderReceived(requiredreceive);
        }
    }
};

template <typename T> void* threadStarter(void* arg) {
    T* cleaner = (T*)arg;
    cleaner->start();
    return (void*)0;
}

int main() {
    bool cont = true;
    vector<Cleaners*> cleaners;
    vector<Smokers*> smokers;
    // Read grid
    int gridx, gridy;
    cin >> gridx >> gridy;
    grid.resize(gridx, vector<int>(gridy, 0));
    grid_Collectors.resize(gridx, vector<int>(gridy, 0));
    grid_Smokers.resize(gridx, vector<int>(gridy, 0));
    grid_SmokingArea.resize(gridx, vector<int>(gridy, 0));
    for (int i = 0; i < gridx; i++) {
        for (int j = 0; j < gridy; j++) {
            cin >> grid[i][j];
        }
    }

    // Read collectors
    int collectorCount;
    cin >> collectorCount;
    for (int i = 0; i < collectorCount; i++) {
        int idx, sizex, sizey, sleepDuration, numberofAreas;
        cin >> idx >> sizex >> sizey >> sleepDuration >> numberofAreas;
        vector<tuple<int, int> > areas(numberofAreas, make_tuple(0, 0));
        for (int j = 0; j < numberofAreas; j++) {
            int x, y;
            cin >> x >> y;
            areas[j] = make_tuple(x, y);
        }
        Cleaners* cleaner = new Cleaners(idx, sizex, sizey, sleepDuration, areas);
        cleaners.push_back(cleaner);
    }

    // Read orders
    vector<tuple<ORDER, int> > orders_sleeps;
    int numberofOrders;
    cin >> numberofOrders;
    if (cin.eof()) {
        cont = false;
    }
    if (cont) {
        orders_sleeps.resize(numberofOrders);
        for (int i = 0; i < numberofOrders; i++) {
            int to;
            string orderstring;
            cin >> to >> orderstring;
            if (orderstring == "break") {
                orders_sleeps[i] = make_tuple(BREAK, to);
            } else if (orderstring == "continue") {
                orders_sleeps[i] = make_tuple(CONTINUE, to);
            } else if (orderstring == "stop") {
                orders_sleeps[i] = make_tuple(STOP, to);
            }
        }

        // Read smokers
        int numberofSmokers;
        cin >> numberofSmokers;
        if (cin.eof()) {
            cont = false;
        }
        if (cont) {
            for (int i = 0; i < numberofSmokers; i++) {
                int idx, timeToSmoke, cellCount;
                cin >> idx >> timeToSmoke >> cellCount;
                vector<tuple<int, int, int> > areas(cellCount);
                for (int j = 0; j < cellCount; j++) {
                    int areax, areay, smokeCount;
                    cin >> areax >> areay >> smokeCount;
                    areas[j] = make_tuple(areax - 1, areay - 1, smokeCount);
                }
                Smokers* smoker = new Smokers(idx, timeToSmoke, areas);
                smokers.push_back(smoker);
            }
        }
    }

    hw2_init_notifier();
    timespec initTime;
    const int gettime_rv = clock_gettime(CLOCK_REALTIME, &initTime);
    if (gettime_rv) {
        cerr << "clock gettime error";
        exit(3);
    }
    for (auto& cleaner : cleaners) {
        cleaner->startThread();
    }
    for (auto& smoker : smokers) {
        smoker->startThread();
    }

    OrderGivers ordergiver(initTime, orders_sleeps);

    for (auto& smoker : smokers) {
        delete smoker;
    }
    for (auto& cleaner : cleaners) {
        delete cleaner;
    }
    return 0;
}
