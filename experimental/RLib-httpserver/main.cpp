#include <LEventLoop.hpp>
// #include <LTcpServer.hpp>
// #include <LHttpServer.hpp>
// #include <LHttpRouter.hpp>

int main() {

    LEventLoop loop;

    // LTcpServer tcpServer;
    // tcpServer.listen(8080);

    // LHttpServer httpServer(tcpServer);

    // LHttpRouter router(httpServer);

    // router.onGet("/api/status", [](const LHttpRequest& req, LHttpResponse& res) {
    //     res.setStatus(200);
    //     res.sendJson("{\"status\": \"OK\"}");
    // });

    return loop.exec();
}
/*
int main()
{
    LEventLoop loop;

    LTcpServer tcpServer;

    LTlsLayer tlsLayer(tcpServer, "cert.pem", "key.pem");

    LHttpServer httpServer(tlsLayer);

    LHttpRouter router(httpServer);

    router.onGet("/api/status", [](const LHttpRequest &req, LHttpResponse &res) {
        res.setStatus(200);
        res.sendJson("{\"status\": \"OK\"}");
    });

    return loop.exec();
}
*/