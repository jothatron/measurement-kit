// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <measurement_kit/ooni.hpp>

#include <event2/dns.h>

#include "../ooni/utils.hpp"

namespace mk {
namespace ooni {
namespace templates {

using namespace mk::report;

void dns_query(Var<Entry> entry, dns::QueryType query_type,
               dns::QueryClass query_class, std::string query_name,
               std::string nameserver, Callback<Error, Var<dns::Message>> cb,
               Settings options, Var<Reactor> reactor, Var<Logger> logger) {

    ErrorOr<net::Endpoint> maybe_epnt = net::parse_endpoint(nameserver, 53);
    if (!maybe_epnt) {
        reactor->call_soon([=]() { cb(maybe_epnt.as_error(), nullptr); });
        return;
    }

    uint16_t resolver_port = maybe_epnt->port;
    std::string resolver_hostname = maybe_epnt->hostname;

    options["dns/nameserver"] = resolver_hostname;
    options["dns/port"] = resolver_port;
    options["dns/engine"] = "libevent"; /* Ensure we use low level engine */
    options["dns/attempts"] = 1;

    dns::query(query_class, query_type, query_name,
               [=](Error error, Var<dns::Message> message) {
                   logger->debug("dns_test: got response!");
                   Entry query_entry;
                   query_entry["resolver_hostname"] = resolver_hostname;
                   query_entry["resolver_port"] = resolver_port;
                   query_entry["failure"] = nullptr;
                   query_entry["answers"] = Entry::array();
                   if (query_type == dns::MK_DNS_TYPE_A) {
                       query_entry["query_type"] = "A";
                       query_entry["hostname"] = query_name;
                   }
                   if (!error) {
                       for (auto answer : message->answers) {
                           if (query_type == dns::MK_DNS_TYPE_A) {
                               query_entry["answers"].push_back(
                                   {{"ttl", answer.ttl},
                                    {"ipv4", answer.ipv4},
                                    {"answer_type", "A"}});
                           }
                       }
                   } else {
                       query_entry["failure"] = error.as_ooni_error();
                   }
                   // TODO add support for bytes received
                   // query_entry["bytes"] = response.get_bytes();
                   (*entry)["queries"].push_back(query_entry);
                   logger->debug("dns_test: callbacking");
                   cb(error, message);
                   logger->debug("dns_test: callback called");
               },
               options, reactor);
}

void http_request(Var<Entry> entry, Settings settings, http::Headers headers,
                  std::string body, Callback<Error, Var<http::Response>> cb,
                  Var<Reactor> reactor, Var<Logger> logger) {

    (*entry)["agent"] = "agent";
    (*entry)["socksproxy"] = nullptr;

    if (settings.find("http/method") == settings.end()) {
        settings["http/method"] = "GET";
    }

    /*
     * XXX probe ip passed down the stack to allow us to scrub it from the
     * entry; see issue #1110 for plans to make this better.
     */
    std::string probe_ip = settings.get("real_probe_ip_", std::string{});
    auto redact = [=](std::string s) {
        if (probe_ip != "" && !settings.get("save_real_probe_ip", false)) {
            s = mk::ooni::scrub(s, probe_ip);
        }
        return s;
    };

    http::request(
        settings, headers, body,
        [=](Error error, Var<http::Response> response) {
            Entry rr;
            /*
             * Note: `probe_ip` comes from an external service, hence
             * we MUST call `represent_string` _after_ `redact()`.
             */
            for (auto pair: headers) {
                rr["request"]["headers"][pair.first] =
                    represent_string(redact(pair.second));
            }
            rr["request"]["body"] = represent_string(redact(body));
            rr["request"]["url"] = settings.at("http/url").c_str();
            rr["request"]["method"] = settings.at("http/method").c_str();

            // XXX we should probably update OONI data format to remove this.
            rr["method"] = settings.at("http/method").c_str();

            if (!error) {
                for (auto pair: response->headers) {
                    rr["response"]["headers"][pair.first] =
                        represent_string(redact(pair.second));
                }
                rr["response"]["body"] =
                    represent_string(redact(response->body));
                rr["response"]["response_line"] =
                    represent_string(redact(response->response_line));
                rr["response"]["code"] = response->status_code;
                rr["failure"] = nullptr;
            } else {
                rr["failure"] = error.as_ooni_error();
            }

            (*entry)["requests"].push_back(rr);
            cb(error, response);
        },
        reactor, logger);
}

void tcp_connect(Settings options, Callback<Error, Var<net::Transport>> cb,
                 Var<Reactor> reactor, Var<Logger> logger) {
    ErrorOr<int> port = options["port"].as_noexcept<int>();
    if (!port) {
        cb(port.as_error(), nullptr);
        return;
    }
    if (options["host"] == "") {
        cb(MissingRequiredHostError(), nullptr);
        return;
    }
    net::connect(options["host"], *port, cb, options, reactor, logger);
}

} // namespace templates
} // namespace ooni
} // namespace mk
