#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cpr/cpr.h>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>

using namespace std;

typedef map<double, double, greater<double>> OrderBookSide; // Using a map to store price levels

struct OrderBook {
    OrderBookSide bids;
    OrderBookSide asks;
};

// Function to fetch snapshot data
OrderBook fetchSnapshot(const string& symbol) {
    string url = "https://api.binance.com/api/v3/depth?symbol=" + symbol + "&limit=100";
    cpr::Response r = cpr::Get(cpr::Url{url});
    OrderBook orderBook;

    if (r.status_code == 200) {
        auto json = nlohmann::json::parse(r.text);

        for (const auto& bid : json["bids"]) {
            double price = stod(bid[0].get<string>());
            double quantity = stod(bid[1].get<string>());
            orderBook.bids[price] = quantity;
        }

        for (const auto& ask : json["asks"]) {
            double price = stod(ask[0].get<string>());
            double quantity = stod(ask[1].get<string>());
            orderBook.asks[price] = quantity;
        }
    } else {
        cerr << "Error fetching snapshot: " << r.status_code << endl;
    }

    return orderBook;
}

// Function to update the order book based on WebSocket updates
void updateOrderBook(OrderBook& orderBook, const nlohmann::json& update) {
    for (const auto& bid : update["b"]) {
        double price = stod(bid[0].get<string>());
        double quantity = stod(bid[1].get<string>());
        if (quantity == 0.0) {
            orderBook.bids.erase(price);
        } else {
            orderBook.bids[price] = quantity;
        }
    }

    for (const auto& ask : update["a"]) {
        double price = stod(ask[0].get<string>());
        double quantity = stod(ask[1].get<string>());
        if (quantity == 0.0) {
            orderBook.asks.erase(price);
        } else {
            orderBook.asks[price] = quantity;
        }
    }
}

// Function to display the top 5 levels of the order book
void displayOrderBook(const OrderBook& orderBook) {
    cout << "Top 5 Bids:" << endl;
    int count = 0;
    for (const auto& [price, quantity] : orderBook.bids) {
        cout << price << " : " << quantity << endl;
        if (++count == 5) break;
    }

    cout << "Top 5 Asks:" << endl;
    count = 0;
    for (const auto& [price, quantity] : orderBook.asks) {
        cout << price << " : " << quantity << endl;
        if (++count == 5) break;
    }
}

// WebSocket message handler
void onMessage(OrderBook& orderBook, websocketpp::connection_hdl hdl, websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::client<websocketpp::config::asio_client>::message_ptr msg) {
    auto json = nlohmann::json::parse(msg->get_payload());
    updateOrderBook(orderBook, json);
    displayOrderBook(orderBook);
}

int main() {
    string symbol;
    cout << "Enter the Binance symbol (e.g., BTCUSDT): ";
    cin >> symbol;

    OrderBook orderBook = fetchSnapshot(symbol);

    // WebSocket setup
    websocketpp::client<websocketpp::config::asio_client> client;
    string uri = "wss://stream.binance.com:9443/ws/" + symbol + "@depth";

    try {
        client.set_access_channels(websocketpp::log::alevel::none);
        client.clear_access_channels(websocketpp::log::alevel::none);

        client.init_asio();

        client.set_message_handler([&orderBook, &client](websocketpp::connection_hdl hdl, websocketpp::client<websocketpp::config::asio_client>::message_ptr msg) {
            onMessage(orderBook, hdl, &client, msg);
        });

        websocketpp::lib::error_code ec;
        auto con = client.get_connection(uri, ec);

        if (ec) {
            cout << "Could not create connection because: " << ec.message() << endl;
            return 1;
        }

        client.connect(con);
        client.run();
    }
    catch (websocketpp::exception const& e) {
        cout << e.what() << endl;
    }
    catch (...) {
        cout << "Some other error" << endl;
    }

    return 0;
}

