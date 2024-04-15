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
long double GAMMA_VALUE = 0.00025;
long double C_VALUE = 5;
long double EPSILON_SENSITIVITY_VALUE = 0.001;

using json = nlohmann::json;

using LabelVector = std::vector<double>;

using Sample = dlib::matrix<double, 5, 1>;
using RadialKernel = dlib::radial_basis_kernel<Sample>;
using Trainer = dlib::svr_trainer<RadialKernel>;

using SampleVector = std::vector<Sample>;
using LinearKernel = dlib::linear_kernel<Sample>;
using DecisionFunction = dlib::decision_function<RadialKernel>;

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

/*
namespace x { using foo = int; }
namespace y { using foo = double; }
void bar() {
    using namespace x;
    using namespace y;
    using x::foo;
    using lib1::http;
    http baz;
}
*/

std::string httpRequest(std::string _endpointUrl) {
    
    try {
        using namespace boost::asio;
        using namespace boost::beast;

        io_context _io_context;
        ssl::context _ssl_context(ssl::context::tlsv12_client);
        _ssl_context.set_default_verify_paths();

        ssl::stream<ip::tcp::socket> _socket(_io_context, _ssl_context);

        ip::tcp::resolver _resolver(_io_context);
        auto const results = _resolver.resolve(BINANCE_BASE_ENDPOINT, "https");

        connect(_socket.next_layer(), results.begin(), results.end());
        _socket.handshake(ssl::stream_base::client);

        http::request<http::empty_body> _http_request;
        _http_request.method(http::verb::get);
        _http_request.target(_endpointUrl);
        _http_request.set(http::field::host, BINANCE_BASE_ENDPOINT);
        _http_request.set(http::field::user_agent, "HTTP Client with BoostBeast");

        http::write(_socket, _http_request);

        flat_buffer _fbuffer;
        http::response<http::dynamic_body> _http_response;

        http::read(_socket, _fbuffer, _http_response);

        int _status_code = _http_response.result_int();

        if (_status_code == 429) {
            std::cout << "BREAKING A REQUEST RATE LIMIT.";
            exit(EXIT_SUCCESS);
        }

        auto _http_response_data = buffers_to_string(_http_response.body().data());

        error_code ec;
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
        char _delimiter;

        while (_inputFileS >> _marketSlice.avgBidPrice >> _delimiter >> _marketSlice.avgAskPrice >> _delimiter >> _marketSlice.totalBidVol >> _delimiter >> _marketSlice.totalAskVol >> _delimiter >> _marketSlice.avgBidAskSpread >> _delimiter >> _marketSlice.priceChange) {
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

void processData(const std::string& _inputFile, SampleVector& _sampleVector, LabelVector& _labelVector) {
    std::ifstream _inputFileS(_inputFile);
    MarketSlice _marketSlice;
    char _delimiter;

    while (_inputFileS >> _marketSlice.avgBidPrice >> _delimiter >> _marketSlice.avgAskPrice >> _delimiter >> _marketSlice.totalBidVol >> _delimiter >> _marketSlice.totalAskVol >> _delimiter >> _marketSlice.avgBidAskSpread >> _delimiter >> _marketSlice.priceChange) {
        _labelVector.emplace_back(_marketSlice.priceChange);
        Sample _sample;
        _sample(0, 0) = _marketSlice.avgBidPrice;
        _sample(1, 0) = _marketSlice.avgAskPrice;
        _sample(2, 0) = _marketSlice.totalBidVol;
        _sample(3, 0) = _marketSlice.totalAskVol;
        _sample(4, 0) = _marketSlice.avgBidAskSpread;
        _sampleVector.emplace_back(_sample);
    }
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

void APS_CrossValidation() {
    LabelVector _labelVector;
    SampleVector _sampleVector;
    Trainer _trainer;

    processData("n_training_data.csv", _sampleVector, _labelVector);
    dlib::randomize_samples(_sampleVector, _labelVector);

    long double _bestGamma = 0;
    long double _bestC = 0;
    long double _bestEpsilonValue = 0;
    long double _bestMSEValue = std::numeric_limits<long double>::max();

    for (long double _gamma = 0.00001; _gamma <= 1; _gamma *= 5) {
        for (long double _c = 1; _c <= 100; _c *= 5) {
            for (long double _epsilon = 0.001; _epsilon <= 0.1; _epsilon *= 5) {
                _trainer.set_kernel(_gamma);
                _trainer.set_c(_c);
                _trainer.set_epsilon_insensitivity(_epsilon);

                auto _results = dlib::cross_validate_regression_trainer(_trainer, _sampleVector, _labelVector, 6);
                long double _MSEValue = _results(0);
                std::cout << "MSE Value: " << _MSEValue << std::endl;

                if (_MSEValue < _bestMSEValue) {
                    _bestMSEValue = _MSEValue;
                    std::cout << "New MSE Value: " << _bestMSEValue << std::endl;
                    _bestGamma = _gamma;
                    _bestC = _c;
                    _bestEpsilonValue = _epsilon;
                }
            }
        }
    }
    GAMMA_VALUE = _bestGamma;
    C_VALUE = _bestC;
    EPSILON_SENSITIVITY_VALUE = _bestEpsilonValue;
    std::cout << "Best MSE Value: " << _bestMSEValue << ", Best Gamma Value: " << _bestGamma << ", Best C Value: " << _bestC << ", Best Epsilon Value: " << _bestEpsilonValue << std::endl;
}

DecisionFunction APS_CreateLFunction() {
    LabelVector _labelVector;
    SampleVector _sampleVector;
    Trainer _trainer;
    _trainer.set_kernel(GAMMA_VALUE);
    _trainer.set_c(C_VALUE);
    _trainer.set_epsilon_insensitivity(EPSILON_SENSITIVITY_VALUE);
    processData("n_training_data.csv", _sampleVector, _labelVector);

    DecisionFunction _learnedFunction = _trainer.train(_sampleVector, _labelVector);
    return _learnedFunction;
}

void APS_Predictor(dlib::matrix<double>& _featureMatrix, DecisionFunction& _learnedFunction) {
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

        dlib::matrix<double> _newFeatures(1, 5);
        _newFeatures(0, 0) = _avgBidPrice;
        _newFeatures(0, 1) = _avgAskPrice;
        _newFeatures(0, 2) = _totalBidVolume;
        _newFeatures(0, 3) = _totalAskVolume;
        _newFeatures(0, 4) = _avgBidAskSpread;

        for (long i = 0; i < _newFeatures.nc(); ++i) {
            _newFeatures(0, i) -= _normMean;
            _newFeatures(0, i) /= _normStandardDeviation;
        }
        long double _prediction = _learnedFunction(_newFeatures);
        double _currentPrice = getPrice();
        double _calculatedPrice = _currentPrice * _prediction;

        std::cout << "I predict that the price will change by: " << _prediction << std::endl;
        std::cout << "So the price will be: " << _calculatedPrice << std::endl;
    }
}

int main() {
    APS_BuildTrainingSet();
    APS_CrossValidation();
    DecisionFunction _learnedFunction = APS_CreateLFunction();
    dlib::matrix<double> _featureMatrix;
    APS_Predictor(_featureMatrix, _learnedFunction);
    std::this_thread::sleep_for(std::chrono::seconds(TIME_WAIT*ITERATION_NUMBER));
    std::cout << "Actual Price: " << getPrice() << std::endl;
    return EXIT_SUCCESS;
}

/*
#if 0
#endif
*/

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

