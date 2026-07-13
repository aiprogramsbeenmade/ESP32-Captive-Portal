#pragma once

#include <DNSServer.h>
#include <IPAddress.h>

/**
 * CaptiveDNS
 * ----------
 * Wrapper minimale attorno a DNSServer che implementa lo step 3 del progetto:
 * risponde a QUALSIASI query DNS in arrivo sulla porta 53 dicendo che il
 * dominio richiesto si trova all'IP dell'Access Point (192.168.4.1).
 * Questo è ciò che fa scattare il "captive portal popup" del sistema operativo
 * dello smartphone (step 4).
 */
class CaptiveDNS {
public:
    // Avvia il server DNS sulla porta 53, rispondendo con apIP a ogni dominio.
    bool start(const IPAddress& apIP);

    // Da chiamare nel loop() principale ad ogni ciclo: processa le richieste in coda.
    void handle();

    void stop();

private:
    DNSServer _dnsServer;
    static constexpr uint16_t DNS_PORT = 53;
};
