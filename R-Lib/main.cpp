#include <cerrno>  // errno
#include <cstring> // strerror
#include <iostream>
#include <sys/epoll.h>   // epoll_create1, epoll_ctl, epoll_wait
#include <sys/timerfd.h> // timerfd_create, timerfd_settime
#include <unistd.h>      // close, read

#include "./lib/LEventLoop.hpp"
#include "./lib/LTimer.hpp"

using namespace std;

int main()
{
    LEventLoop loop;

    LTimer timer;
    timer.start();

    // 1. Tworzenie file descriptora dla timera
    // CLOCK_MONOTONIC - zegar, który zawsze idzie do przodu (odporny na zmiane czasu systemowego)
    // TFD_NONBLOCK - tryb nieblokujący (ważne dla epoll)
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd == -1) {
        cerr << "Błąd timerfd_create: " << strerror(errno) << endl;
        return 1;
    }

    // 2. Konfiguracja czasu timera
    struct itimerspec ts;

    // it_value - czas pierwszego uruchomienia (za 1 sekundę)
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = (5 * 1000);

    // it_interval - co ile ma się powtarzać (co 1 sekundę)
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = (5 * 1000);

    if (timerfd_settime(timer_fd, 0, &ts, NULL) == -1) {
        cerr << "Błąd timerfd_settime: " << strerror(errno) << endl;
        close(timer_fd);
        return 1;
    }

    // 3. Tworzenie instancji epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        cerr << "Błąd epoll_create1: " << strerror(errno) << endl;
        close(timer_fd);
        return 1;
    }

    // 4. Dodanie timer_fd do epoll
    struct epoll_event event;
    event.events = EPOLLIN; // Interesuje nas odczyt (gdy timer "tyknie")
    event.data.fd = timer_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1) {
        cerr << "Błąd epoll_ctl: " << strerror(errno) << endl;
        close(timer_fd);
        close(epoll_fd);
        return 1;
    }

    cout << "Timer wystartował. Czekam na zdarzenia..." << endl;

    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    // 5. Główna pętla
    while (true) {
        // Czekaj na zdarzenia (timeout -1 oznacza czekanie w nieskończoność)
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if (n == -1) {
            cerr << "Błąd epoll_wait: " << strerror(errno) << endl;
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == timer_fd) {
                // WAŻNE: Musimy odczytać dane z timera, aby wyczyścić stan gotowości.
                // Timerfd zwraca uint64_t reprezentujący liczbę wygaśnięć od ostatniego odczytu.
                uint64_t expirations;
                ssize_t s = read(timer_fd, &expirations, sizeof(expirations));

                if (s != sizeof(expirations)) {
                    cerr << "Błąd odczytu z timera" << endl;
                } else {
                    // Właściwa akcja

                    static uint32_t print_allow_flag = 0;
                    if (print_allow_flag++ % 100000 == 0) {
                        cout << "Expirations: " << expirations << endl;
                        cout << "Tik! Minęła sekunda. (Liczba wygaśnięć: " << expirations << ")"
                             << endl;
                    }
                }
            }
        }
    }

    // Sprzątanie (choć OS zrobi to przy wyjściu z programu)
    close(timer_fd);
    close(epoll_fd);

    loop.exec();

    return 0;
}
