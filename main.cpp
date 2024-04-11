#include <iostream>
#include <fstream>
#include <cstdlib>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>
#include <dlib/svm.h>
#include <dlib/matrix.h>

const std::string BINANCE_BASE_ENDPOINT = "api.binance.com";
const std::string BINANCE_TIME_ENDPOINT = "/api/v3/time";
const std::string BINANCE_CANDLESTICK_ENDPOINT = "/api/v3/klines";
const std::string BINANCE_ORDERBOOK_ENDPOINT = "/api/v3/depth";
const std::string BINANCE_RECENT_TRADES_ENDPOINT = "/api/v3/trades";
const std::string BINANCE_PORT = "443";
const std::string API_KEY = "";
const std::string API_SECRET = "";
const std::string SYMBOL = "BTCUSDT";
const int ITERATION_NUMBER = 20;
const int TIME_WAIT = 30;

using json = nlohmann::json;

void writeToCSV(std::string _filename, double _avgBidPrice, double _avgAskPrice, double _totalBidVolume, double _totalAskVolume, double _avgBidAskSpread, long double _priceChange) {
    std::ofstream _file(_filename, std::ios::app);
    if (_file.is_open()) {
        _file << _avgBidPrice << ",";
        _file << _avgAskPrice << ",";
        _file << _totalBidVolume << ",";
        _file << _totalAskVolume << ",";
        _file << _avgBidAskSpread << ",";
        _file << _priceChange << "\n";
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

std::string constructGetPriceURL(std::string _base_url, std::string _symbol, long _limit) {
    std::string _endpointUrl = _base_url + "?symbol=" + _symbol + "&limit=" + std::to_string(_limit);
    return _endpointUrl;
}

std::string httpRequest(std::string _endpointUrl) {
    
    try {
        boost::asio::io_context _io_context;
        boost::asio::ssl::context _ssl_context(boost::asio::ssl::context::tlsv12_client);
        _ssl_context.set_default_verify_paths();

        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> _socket(_io_context, _ssl_context);

        boost::asio::ip::tcp::resolver _resolver(_io_context);
        auto const results = _resolver.resolve(BINANCE_BASE_ENDPOINT, "https");

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

double totalBidPrice(const json& _jsonResponse) {
    double _totalBidPrice = 0;
    for (const auto& _bid : _jsonResponse["bids"]) {
        double _bidPrice = std::stod(_bid[0].get<std::string>());
        double _bidQty = std::stod(_bid[1].get<std::string>());
        _totalBidPrice += (_bidPrice*_bidQty);
    }
    return _totalBidPrice;
}

double totalBidVol(const json& _jsonResponse) {
    double _totalBidVol = 0;
    for (const auto& _bid : _jsonResponse["bids"]) {
        double _bidQty = std::stod(_bid[1].get<std::string>());
        _totalBidVol += _bidQty;
    }
    return _totalBidVol;
}

double avgBidPrice(const json& _jsonResponse) {
    double _avgBidPrice = 0;
    double _totalBidPrice = totalBidPrice(_jsonResponse);
    double _totalBidVolume = totalBidVol(_jsonResponse);
    _avgBidPrice = _totalBidPrice / _totalBidVolume;
    return _avgBidPrice;
}

double totalAskPrice(const json& _jsonResponse) {
    double _totalAskPrice = 0;
    for (const auto& _ask : _jsonResponse["asks"]) {
        double _askPrice = std::stod(_ask[0].get<std::string>());
        double _askQty = std::stod(_ask[1].get<std::string>());
        _totalAskPrice += (_askPrice * _askQty);
    }
    return _totalAskPrice;
}

double totalAskVol(const json& _jsonResponse) {
    double _totalAskVol = 0;
    std::vector<double> _askVolumes;
    for (const auto& _ask : _jsonResponse["asks"]) {
        double _askQty = std::stod(_ask[1].get<std::string>());
        _totalAskVol += _askQty;
    }
    return _totalAskVol;
}

double avgAskPrice(const json& _jsonResponse) {
    double _avgAskPrice = 0;
    double _totalAskPrice = totalAskPrice(_jsonResponse);
    double _totalAskVolume = totalAskVol(_jsonResponse);
    _avgAskPrice = _totalAskPrice / _totalAskVolume;
    return _avgAskPrice;
}

double avgBidAskSpread(const json& _jsonResponse) {
    double _avgBidAskSpread = 0;
    double _avgBidPrice = 0;
    double _avgAskPrice = 0;

    _avgBidPrice = avgBidPrice(_jsonResponse);
    _avgAskPrice = avgAskPrice(_jsonResponse);

    _avgBidAskSpread = _avgBidPrice - _avgAskPrice;
    return _avgBidAskSpread;
}

double getPrice() {
    double _price = 0;
    std::string _endpointUrl = constructGetPriceURL(BINANCE_RECENT_TRADES_ENDPOINT, SYMBOL, 1);
    std::string _http_response_data = httpRequest(_endpointUrl);
    if (!_http_response_data.empty()) {
        json _jsonResponse = json::parse(_http_response_data);
        for (const auto& _slice : _jsonResponse[0]["price"]) {
            _price = std::stod(_slice.get<std::string>());
        }
    }
    return _price;
}

struct MarketSlice {
    double avgBidPrice;
    double avgAskPrice;
    double totalBidVol;
    double totalAskVol;
    double avgBidAskSpread;
    long double priceChange;
};

void dataNormalizer(const std::string& _inputFile, const std::string& _outputFile) {
    std::ifstream _inputFileS(_inputFile);
    std::ofstream _outputFileS(_outputFile);
    
    if (_inputFileS.is_open() && _outputFileS.is_open()) {

        std::vector<MarketSlice> _marketSlices;
        MarketSlice _marketSlice;
        char _buffer;

        while (_inputFileS >> _marketSlice.avgBidPrice >> _buffer >> _marketSlice.avgAskPrice >> _buffer >> _marketSlice.totalBidVol >> _buffer >> _marketSlice.totalAskVol >> _buffer >> _marketSlice.avgBidAskSpread >> _buffer >> _marketSlice.priceChange) {
            _marketSlices.push_back(_marketSlice);
        }

        double _minAvgBidPrice = _marketSlices[0].avgBidPrice;
        double _maxAvgBidPrice = _marketSlices[0].avgBidPrice;

        double _minAvgAskPrice = _marketSlices[0].avgAskPrice;
        double _maxAvgAskPrice = _marketSlices[0].avgAskPrice;

        double _minTotalBidVol = _marketSlices[0].totalBidVol;
        double _maxTotalBidVol = _marketSlices[0].totalBidVol;

        double _minTotalAskVol = _marketSlices[0].totalAskVol;
        double _maxTotalAskVol = _marketSlices[0].totalAskVol;

        double _minAvgBidAskSpread = _marketSlices[0].avgBidAskSpread;
        double _maxAvgBidAskSpread = _marketSlices[0].avgBidAskSpread;

        for (const auto& _slice : _marketSlices) {

            if (_slice.avgBidPrice < _minAvgBidPrice) {
                _minAvgBidPrice = _slice.avgBidPrice;
            }
            else if (_slice.avgBidPrice > _maxAvgBidPrice) {
                _maxAvgBidPrice = _slice.avgBidPrice;
            }

            if (_slice.avgAskPrice < _minAvgAskPrice) {
                _minAvgAskPrice = _slice.avgAskPrice;
            }
            else if (_slice.avgAskPrice > _maxAvgAskPrice) {
                _maxAvgAskPrice = _slice.avgAskPrice;
            }

            if (_slice.totalBidVol < _minTotalBidVol) {
                _minTotalBidVol = _slice.totalBidVol;
            }
            else if (_slice.totalBidVol > _maxTotalBidVol) {
                _maxTotalBidVol = _slice.totalBidVol;
            }

            if (_slice.totalAskVol < _minTotalAskVol) {
                _minTotalAskVol = _slice.totalAskVol;
            }
            else if (_slice.totalAskVol > _maxTotalAskVol) {
                _maxTotalAskVol = _slice.totalAskVol;
            }

            if (_slice.avgBidAskSpread < _minAvgBidAskSpread) {
                _minAvgBidAskSpread = _slice.avgBidAskSpread;
            }
            else if (_slice.avgBidAskSpread > _maxAvgBidAskSpread) {
                _maxAvgBidAskSpread = _slice.avgBidAskSpread;
            }
        }

        for (const auto& _slice : _marketSlices) {
            double _normalizedAvgBidPrice = (_slice.avgBidPrice - _minAvgBidPrice) / (_maxAvgBidPrice - _minAvgBidPrice);
            double _normalizedAvgAskPrice = (_slice.avgAskPrice - _minAvgAskPrice) / (_maxAvgAskPrice - _minAvgAskPrice);

            double _normalizedTotalBidVolume = (_slice.totalBidVol - _minTotalBidVol) / (_maxTotalBidVol - _minTotalBidVol);
            double _normalizedTotalAskVolume = (_slice.totalAskVol - _minTotalAskVol) / (_maxTotalAskVol - _minTotalAskVol);

            double _normalizedBidAskSpread = (_slice.avgBidAskSpread - _minAvgBidAskSpread) / (_maxAvgBidAskSpread - _minAvgBidAskSpread);

            long double _priceChange = _slice.priceChange;

            _outputFileS << _normalizedAvgBidPrice << "," << _normalizedAvgAskPrice << "," << _normalizedTotalBidVolume << "," << _normalizedTotalAskVolume << "," << _normalizedBidAskSpread << "," << _priceChange << std::endl;
        }
        std::cout << "Normalized " << _inputFile << "." << std::endl;
    }
    else {
        std::cerr << "Failed to Open Input or Output File." << std::endl;
    }
}

dlib::matrix<double> processData(const std::string& _inputFile, dlib::matrix<double>& _featureMatrix, std::vector<double>& _labelVector) {
    int _numberOfFeatures = 5;
    std::ifstream _inputFileS(_inputFile);
    std::vector<MarketSlice> _marketSlices;
    MarketSlice _marketSlice;
    char _buffer;

    while (_inputFileS >> _marketSlice.avgBidPrice >> _buffer >> _marketSlice.avgAskPrice >> _buffer >> _marketSlice.totalBidVol >> _buffer >> _marketSlice.totalAskVol >> _buffer >> _marketSlice.avgBidAskSpread >> _buffer >> _marketSlice.priceChange) {
        _marketSlices.push_back(_marketSlice);
    }
    int _matrixLength = _marketSlices.size();
    _labelVector.resize(_matrixLength);

    dlib::matrix<double> _dlibFeatureMatrix(_matrixLength, _numberOfFeatures);

    for (int i = 0; i < _matrixLength; i++) {
        _dlibFeatureMatrix(i, 0) = _marketSlices[i].avgBidPrice;
        _dlibFeatureMatrix(i, 1) = _marketSlices[i].avgAskPrice;
        _dlibFeatureMatrix(i, 2) = _marketSlices[i].totalBidVol;
        _dlibFeatureMatrix(i, 3) = _marketSlices[i].totalAskVol;
        _dlibFeatureMatrix(i, 4) = _marketSlices[i].avgBidAskSpread;
        _labelVector[i] = _marketSlices[i].priceChange;
    }
    return _dlibFeatureMatrix;
}

void APS_BuildTrainingSet() {
    long _limit = 10;
    std::string _endpointUrl = constructOrderBookUrl(BINANCE_ORDERBOOK_ENDPOINT, SYMBOL, _limit);

    std::vector<MarketSlice> _trainingDataSet(ITERATION_NUMBER);
    std::vector<double> _oldPrice;

    for (int i = 0; i < ITERATION_NUMBER; i++) {
        std::string _http_response_data = httpRequest(_endpointUrl);

        if (!_http_response_data.empty()) {
            json _jsonResponse = json::parse(_http_response_data);

            _trainingDataSet[i].avgBidPrice = avgBidPrice(_jsonResponse);
            _trainingDataSet[i].avgAskPrice = avgAskPrice(_jsonResponse);
            _trainingDataSet[i].totalBidVol = totalBidVol(_jsonResponse);
            _trainingDataSet[i].totalAskVol = totalAskVol(_jsonResponse);
            _trainingDataSet[i].avgBidAskSpread = avgBidAskSpread(_jsonResponse);
            _oldPrice.push_back(getPrice());

            std::cout << "Current Price: " << getPrice() << std::endl;
            std::cout << "Iteration for orderbook: " << std::to_string(i) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(TIME_WAIT));
    }

    for (int i = 0; i < ITERATION_NUMBER; i++) {
        _trainingDataSet[i].priceChange = getPrice() / _oldPrice[i];
        std::cout << "Old Price: " << _oldPrice[i] << std::endl;
        std::cout << "Current Price: " << getPrice() << std::endl;
        std::cout << "Price Change %: " << _trainingDataSet[i].priceChange << std::endl;
        std::cout << "Iteration for pricing: " << std::to_string(i) << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(TIME_WAIT));
    }
    
    for (int i = 0; i < _trainingDataSet.size(); i++) {
        writeToCSV("training_data.csv", _trainingDataSet[i].avgBidPrice, _trainingDataSet[i].avgAskPrice, _trainingDataSet[i].totalBidVol, _trainingDataSet[i].totalAskVol, _trainingDataSet[i].avgBidAskSpread, _trainingDataSet[i].priceChange);
    }
    dataNormalizer("training_data.csv", "n_training_data.csv");
}

dlib::decision_function<dlib::linear_kernel<double>> APS_CreateLFunction() {
    std::vector<double> _labelVector;
    dlib::matrix<double> _featureMatrix;
    processData("n_training_data.csv", _featureMatrix, _labelVector);

    dlib::svm_c_trainer<dlib::linear_kernel<double>> _svmTrainer;
    dlib::decision_function<dlib::linear_kernel<double>> _learnedFunction = _svmTrainer.train(_featureMatrix, _labelVector);

    return _learnedFunction;
}
// NOT FINISHED
void APS_Predictor(dlib::matrix<double>& _featureMatrix, dlib::decision_function<dlib::linear_kernel<double>>& _learnedFunction) {
    double _normMean = dlib::mean(dlib::mat(_featureMatrix));
    double _normStandardDeviation = dlib::stddev(dlib::mat(_featureMatrix));

    std::string _endpointUrl = constructOrderBookUrl(BINANCE_ORDERBOOK_ENDPOINT, SYMBOL, 1);
    std::string _http_response_data = httpRequest(_endpointUrl);
    if (!_http_response_data.empty()) {
        json _jsonResponse = json::parse(_http_response_data);

        double _avgBidPrice = avgBidPrice(_jsonResponse);
        double _avgAskPrice = avgAskPrice(_jsonResponse);
        double _totalBidVolume = totalBidVol(_jsonResponse);
        double _totalAskVolume = totalAskVol(_jsonResponse);
        double _avgBidAskSpread = avgBidAskSpread(_jsonResponse);

        dlib::matrix<double> _dlibNewFeatures(1, 5);
        _dlibNewFeatures(0, 0) = _avgBidPrice;
        _dlibNewFeatures(0, 1) = _avgBidPrice;
        _dlibNewFeatures(0, 2) = _avgBidPrice;
        _dlibNewFeatures(0, 3) = _avgBidPrice;
        _dlibNewFeatures(0, 4) = _avgBidPrice;
    }
}

int main() {
    APS_BuildTrainingSet();
    dlib::decision_function<dlib::linear_kernel<double>> _learnedFunction = APS_CreateLFunction();
    dlib::matrix<double> _featureMatrix;
    APS_Predictor(_featureMatrix, _learnedFunction);
    return EXIT_SUCCESS;
}
    /*
    get order book data, then get price x amount of time from that point < calculate price% change is then the label
    1.0) data collection ORDER BOOK DATA + cur price
    1.1) wait 5 minutes then get cur price, calculate %price change and add to csv
    1.2) normalize new produced data set
    2) training, execute svm with produced data set to create learning function
    3) prediction, get order book information, apply function, produce price prediction
    
    order book + price vs price 5 minutes from then +  %price change
    
    0
    5 minutes process learning
    predict price 5 minutes in the future
    - price -

    // CLEAN DATA.CSV HERE, PRODUCE DATA_CLEANED.CSV
    // CREATE ML CODE AROUND THRESHOLD, REWARD IS START PRICE OF OPERATION VS FINAL PRICE OF OPERATION
        
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

    */

