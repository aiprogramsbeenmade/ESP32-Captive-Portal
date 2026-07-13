#include "CaptiveDNS.h"

// Definizione fuori dalla classe richiesta per la static constexpr DNS_PORT
// quando viene odr-usata (passata per riferimento/indirizzo) da DNSServer::start.
constexpr uint16_t CaptiveDNS::DNS_PORT;

bool CaptiveDNS::start(const IPAddress& apIP) {
    // NoError + "*" = risponde a ogni dominio richiesto con l'IP dell'AP.
    // E' questo il meccanismo di "DNS spoofing" che genera il redirect captive portal.
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    return _dnsServer.start(DNS_PORT, "*", apIP);
}

void CaptiveDNS::handle() {
    _dnsServer.processNextRequest();
}

void CaptiveDNS::stop() {
    _dnsServer.stop();
}
