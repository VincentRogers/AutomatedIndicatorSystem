#include <iostream>
#include <fstream>
#include <cstdlib>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>

/*
Data Cleaning
SMA integration
OrderBook Analysis COMPARE VOLUME FOR THE PAST 5 MINUTES
WRITEUP
TensorFlow..?
*/

const std::string BINANCE_BASE_ENDPOINT = "api.binance.com";
const std::string BINANCE_TIME_ENDPOINT = "/api/v3/time";
const std::string BINANCE_CANDLESTICK_ENDPOINT = "/api/v3/klines";
const std::string BINANCE_ORDERBOOK_ENDPOINT = "/api/v3/depth";
const std::string BINANCE_PORT = "443";
const std::string CURRENCY_SYMBOL = "BTC";
const std::string API_KEY = "";
const std::string API_SECRET = "";

using json = nlohmann::json;

std::string cleanData(std::string _data) {
    return "";
}

void writeToCSV(std::string _filename, double _avgBidPrice, double _avgAskPrice, double _totalBidVolume, double _totalAskVolume, double _avgBidAskSpread) {
    std::ofstream _file(_filename, std::ios::app);
    if (_file.is_open()) {
        _file << _avgBidPrice << ",";
        _file << _avgAskPrice << ",";
        _file << _totalBidVolume << ",";
        _file << _totalAskVolume << ",";
        _file << _avgBidAskSpread << "\n";
        _file.close();
        std::cout << "Written to File." << std::endl;
    }
    else {
        std::cout << "Could not Open file." << std::endl;
    }
}


std::string constructCandlestickUrl(std::string _base_url, std::string _symbol, long _timeInterval, std::string _intervalType) {
    std::string _endpointUrl = _base_url + "?symbol=" + _symbol + "&interval=" + std::to_string(_timeInterval) + _intervalType;
    return _endpointUrl;
}

std::string constructOrderBookUrl(std::string _base_url, std::string _symbol, long _limit) {
    std::string _endpointUrl = _base_url + "?symbol=" + _symbol + "&limit=" + std::to_string(_limit);
    return _endpointUrl;
}

std::string httpRequest(std::string _endpointUrl) {
    
    try {
        std::cout << "Boost version: " << BOOST_LIB_VERSION << std::endl;

        boost::asio::io_context _io_context;
        boost::asio::ssl::context _ssl_context(boost::asio::ssl::context::tlsv12_client);
        _ssl_context.set_default_verify_paths();

        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> _socket(_io_context, _ssl_context);

        std::cout << "Resolving given endpoint." << std::endl;
        boost::asio::ip::tcp::resolver _resolver(_io_context);
        auto const results = _resolver.resolve(BINANCE_BASE_ENDPOINT, "https");

        std::cout << "Attempting SSL Handshake." << std::endl;
        boost::asio::connect(_socket.next_layer(), results.begin(), results.end());
        _socket.handshake(boost::asio::ssl::stream_base::client);

        boost::beast::http::request<boost::beast::http::empty_body> _http_request;
        _http_request.method(boost::beast::http::verb::get);
        _http_request.target(_endpointUrl);
        _http_request.set(boost::beast::http::field::host, BINANCE_BASE_ENDPOINT);
        _http_request.set(boost::beast::http::field::user_agent, "HTTP Client with BoostBeast");

        boost::beast::http::write(_socket, _http_request);

        boost::beast::flat_buffer _fbuffer;
        boost::beast::http::response<boost::beast::http::dynamic_body> _http_response;

        boost::beast::http::read(_socket, _fbuffer, _http_response);

        int _status_code = _http_response.result_int();

        if (_status_code == 429) {
            std::cout << "BREAKING A REQUEST RATE LIMIT.";
            exit(EXIT_SUCCESS);
        }

        auto _http_response_data = boost::beast::buffers_to_string(_http_response.body().data());

        boost::beast::error_code ec;
        _socket.shutdown(ec);

        return _http_response_data;

    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return "";
    }
}

double totalBidPrice(json& _jsonResponse) {
    double _totalBidPrice = 0;
    for (const auto& _bid : _jsonResponse["bids"]) {
        double _bidPrice = std::stod(_bid[0].get<std::string>());
        double _bidQty = std::stod(_bid[1].get<std::string>());
        _totalBidPrice += (_bidPrice*_bidQty);
    }
    return _totalBidPrice;
}

double totalBidVol(json& _jsonResponse) {
    double _totalBidVol = 0;
    for (const auto& _bid : _jsonResponse["bids"]) {
        double _bidQty = std::stod(_bid[1].get<std::string>());
        _totalBidVol += _bidQty;
    }
    return _totalBidVol;
}

double avgBidPrice(json& _jsonResponse) {
    double _avgBidPrice = 0;
    double _totalBidPrice = totalBidPrice(_jsonResponse);
    double _totalBidVolume = totalBidVol(_jsonResponse);
    _avgBidPrice = _totalBidPrice / _totalBidVolume;
    return _avgBidPrice;
}

double totalAskPrice(json& _jsonResponse) {
    double _totalAskPrice = 0;
    for (const auto& _ask : _jsonResponse["asks"]) {
        double _askPrice = std::stod(_ask[0].get<std::string>());
        double _askQty = std::stod(_ask[1].get<std::string>());
        _totalAskPrice += (_askPrice * _askQty);
    }
    return _totalAskPrice;
}

double totalAskVol(json& _jsonResponse) {
    double _totalAskVol = 0;
    std::vector<double> _askVolumes;
    for (const auto& _ask : _jsonResponse["asks"]) {
        double _askQty = std::stod(_ask[1].get<std::string>());
        _totalAskVol += _askQty;
    }
    return _totalAskVol;
}

double avgAskPrice(json& _jsonResponse) {
    double _avgAskPrice = 0;
    double _totalAskPrice = totalAskPrice(_jsonResponse);
    double _totalAskVolume = totalAskVol(_jsonResponse);
    _avgAskPrice = _totalAskPrice / _totalAskVolume;
    return _avgAskPrice;
}

double avgBidAskSpread(json& _jsonResponse) {
    double _avgBidAskSpread = 0;
    double _avgBidPrice = 0;
    double _avgAskPrice = 0;

    _avgBidPrice = avgBidPrice(_jsonResponse);
    _avgAskPrice = avgAskPrice(_jsonResponse);

    _avgBidAskSpread = _avgBidPrice - _avgAskPrice;
    return _avgBidAskSpread;
}

int main() {
    /*
    std::string _symbol = "BTC-240328-70000-C";
    long _timeInterval = 1;
    std::string _intervalType = "m";
    std::string _endpointUrl = constructCandlestickUrl(BINANCE_CANDLESTICK_ENDPOINT, _symbol, _timeInterval, _intervalType);
    */

    std::string _symbol = "BTCUSDT";
    long _limit = 10;
    
    std::string _endpointUrl = constructOrderBookUrl(BINANCE_ORDERBOOK_ENDPOINT, _symbol, _limit);

    for (int i = 0; i != 30; i++) {
        std::string _http_response_data = httpRequest(_endpointUrl);

        if (!_http_response_data.empty()) {

            json _jsonResponse = json::parse(_http_response_data);
            json& _jsonResponseRef = _jsonResponse;

            double _avgBidPrice = avgBidPrice(_jsonResponseRef);
            double _avgAskPrice = avgAskPrice(_jsonResponseRef);
            double _totalBidVolume = totalBidVol(_jsonResponseRef);
            double _totalAskVolume = totalAskVol(_jsonResponseRef);
            double _avgBidAskSpread = avgBidAskSpread(_jsonResponseRef);

            writeToCSV("data.csv", _avgBidPrice, _avgAskPrice, _totalBidVolume, _totalAskVolume, _avgBidAskSpread);

            std::cout << "Iteration: " << std::to_string(i) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // data.csv organization: sum of bidding volume, sum of asking volume, bid-ask spread average, SMA, 
        // make formula that creates signal BEAR or BULL
        

        // bid volume / total volume = percentage of bid volume
        // ask volume / total volume = percentage of ask volume
        
        // trade_threshold = %volume * bid-ask spread weight

        // trade_threshold variable ML, e.g. 20-80 is deadzone, sub 20 is BEAR, over 80 is BULL.
        // trade_threshold if its 50% is over-sensitive, over-trading.

        // reward is accuracy of indicator, if prediction is close to correct or not close to correct.
        // success = if price moves upward after BULL signal generated, if price moves downward after BEAR signal generated.
        // price at signal generation, price at timeframe after prediction price1 / avg price over elapsed time 

        // price at signal generation, % increase is success measurement

        // e.g. if buy signal, and price increase 10% then success, relative percentage = amount of success

        // trade confidence calculated by narrow spread vs wide spread, current spread vs average overall spread in timeframe (e.g. monthly spread average)
    }



    // 30 intervals of 10 seconds for total runtime of 5 minutes
    
    // calculate the SMA of the past 5 minutes, sum of closing prices of past 5 minutes / number of periods in timeframe, e.g. 30
    // calculate the closing price average of the past 5 minutes
    // check price movement direction over an hour (12 * 5 minutes = 60 minutes)
    // clean data of summary of past 5 minutes
 
    // Web based interface, data visualization, candlestick for 10 second interval
 
    // Bid Volume vs Ask Volume - Market Sentiment
    // More buyers than sellers, indicates bullish movement
    // More sellers than buyers, indicates bearish movement
    
    // Bid-Ask Spread = Ask Price - Bid Price
    // Narrow Spread = higher liquidity
    // Wide Spread = lower liquidity
    // High Liquidity = strong confidence in asset
    // Low Liquidity = weak confidence in asset

    // Review csv file, and run ML on collected data.

    return EXIT_SUCCESS;
}
