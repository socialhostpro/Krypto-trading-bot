#ifndef K_IF_H_
#define K_IF_H_

namespace K {
  static class Screen {
    public:
      Screen() {
        cout << BGREEN << "K"
             << RGREEN << " build " << K_BUILD << ' ' << K_STAMP
             << ".\n";
        string changes;
        int commits = -1;
        if (mCommand::git()) {
          mCommand::fetch();
          changes = mCommand::changelog();
          commits = count(changes.begin(), changes.end(), '\n');
        }
        cout << BGREEN << K_0_DAY << RGREEN << ' ' << (commits == -1
          ? "(zip install)"
          : (commits
            ? '-' + to_string(commits) + "commit" + (commits == 1?"":"s") + '.'
            : "(0day)"
          )
        )
#ifndef NDEBUG
        << " with DEBUG MODE enabled"
#endif
        << ".\n" << RYELLOW << changes << RRESET;
      };
      virtual void pressme(const mHotkey&, function<void()>) = 0;
      virtual void printme(mToScreen *const) = 0;
      virtual void waitForUser() = 0;
      virtual const int error(const string&, const string&, const bool& = false) = 0;
      virtual const string stamp() = 0;
      virtual void logWar(string, string, string = " Warrrrning: ") = 0;
      virtual void logUI(const string&) = 0;
      virtual void logUIsess(int, string) = 0;
      virtual void log(const string&, const string&, const string& = "") = 0;
#define PRETTY_DEBUG if (args.debugEvents) screen->log("DEBUG EV", __PRETTY_FUNCTION__);
      virtual void end() = 0;
  } *screen = nullptr;

  static class Events {
    public:
      virtual void deferred(const function<void()>&) = 0;
  } *events = nullptr;

  static class Sqlite {
    public:
      virtual void backup(mFromDb *const) = 0;
  } *sqlite = nullptr;

  static class Client {
    public:
      uWS::Hub* socket = nullptr;
      virtual void timer_Xs() = 0;
      virtual void welcome(mToClient&) = 0;
      virtual void clickme(mFromClient&, function<void(const json&)>) = 0;
  } *client = nullptr;

  class GwExchangeData {
    public:
      function<void(const mOrder&)>        write_mOrder;
      function<void(const mTrade&)>        write_mTrade;
      function<void(const mLevels&)>       write_mLevels;
      function<void(const mWallets&)>      write_mWallets;
      function<void(const mConnectivity&)> write_mConnectivity;
#define RAWDATA_ENTRY_POINT(mData, read) write_##mData = [&](const mData &rawdata) read
      bool askForFees = false;
      const bool *askForCancelAll = nullptr;
      const mRandId (*randId)() = nullptr;
      virtual const json handshake() = 0;
      virtual const bool askForData(const unsigned int &tick) = 0;
      virtual const bool waitForData() = 0;
      void place(const mOrder *const order) {
        place(
          order->orderId,
          order->side,
          str8(order->price),
          str8(order->quantity),
          order->type,
          order->timeInForce,
          order->preferPostOnly
        );
      };
//BO non-free gw library functions from build-*/local/lib/K-*.a (it just redefines all virtual gateway class members below).
/**/  virtual bool ready() = 0;                                              // wait for exchange and register data handlers
/**/  function<void(mRandId, string)> replace;                               // call         async orders data from exchange
/**/  virtual void place(mRandId, mSide, string, string, mOrderType, mTimeInForce, bool) = 0, // async orders as above/below
/**/               cancel(mRandId, mRandId) = 0,                             // call         async orders data from exchange
/**/               close() = 0;                                              // disconnect but without waiting for reconnect
/**/protected:
/**/  virtual bool            async_wallet() { return false; };              // call         async wallet data from exchange
/**/  virtual vector<mWallets> sync_wallet() { return {}; };                 // call and read sync wallet data from exchange
/**/  virtual vector<mLevels>  sync_levels()  { return {}; };                // call and read sync levels data from exchange
/**/  virtual vector<mTrade>   sync_trades()  { return {}; };                // call and read sync trades data from exchange
/**/  virtual vector<mOrder>   sync_orders()  { return {}; };                // call and read sync orders data from exchange
/**/  virtual vector<mOrder>   sync_cancelAll() = 0;                         // call and read sync orders data from exchange
//EO non-free gw library functions from build-*/local/lib/K-*.a (it just redefines all virtual gateway class members above).
      future<vector<mWallets>> replyWallets;
      future<vector<mLevels>> replyLevels;
      future<vector<mTrade>> replyTrades;
      future<vector<mOrder>> replyOrders;
      future<vector<mOrder>> replyCancelAll;
      const bool askForNeverAsyncData(const unsigned int &tick) {
        bool waiting = false;
        if (TRUEONCE(askForFees)
          or !(tick % 15))       waiting |= !(async_wallet() or !askFor(replyWallets, [&]() { return sync_wallet(); }));
        if (*askForCancelAll
          and !(tick % 300))     waiting |= askFor(replyCancelAll, [&]() { return sync_cancelAll(); });
        return waiting;
      };
      const bool askForSyncData(const unsigned int &tick) {
        bool waiting = false;
        if (!(tick % 2))         waiting |= askFor(replyOrders, [&]() { return sync_orders(); });
                                 waiting |= askForNeverAsyncData(tick);
        if (!(tick % 3))         waiting |= askFor(replyLevels, [&]() { return sync_levels(); });
        if (!(tick % 60))        waiting |= askFor(replyTrades, [&]() { return sync_trades(); });
        return waiting;
      };
      const bool waitForNeverAsyncData() {
        return waitFor(replyWallets,   write_mWallets)
             | waitFor(replyCancelAll, write_mOrder);
      };
      const bool waitForSyncData() {
        return waitFor(replyOrders,    write_mOrder)
             | waitForNeverAsyncData()
             | waitFor(replyLevels,    write_mLevels)
             | waitFor(replyTrades,    write_mTrade);
      };
      template<typename mData, typename syncFn> const bool askFor(
              future<vector<mData>> &reply,
        const syncFn                &read
      ) {
        bool waiting = reply.valid();
        if (!waiting) {
          reply = ::async(launch::async, read);
          waiting = true;
        }
        return waiting;
      };
      template<typename mData> const unsigned int waitFor(
              future<vector<mData>>        &reply,
        const function<void(const mData&)> &write
      ) {
        bool waiting = reply.valid();
        if (waiting and reply.wait_for(chrono::nanoseconds(0))==future_status::ready) {
          for (mData &it : reply.get()) write(it);
          waiting = false;
        }
        return waiting;
      };
  };

  class GwExchange: public GwExchangeData,
                    public mToScreen {
    public:
      uWS::Hub *socket  = nullptr;
      mMonitor *monitor = nullptr;
      unsigned int countdown = 0;
         string exchange = "", symbol  = "",
                apikey   = "", secret  = "",
                user     = "", pass    = "",
                ws       = "", http    = "";
        mCoinId base     = "", quote   = "";
         mPrice minTick  = 0;
        mAmount makeFee  = 0, takeFee  = 0,
                minSize  = 0;
            int version  = 0, maxLevel = 0,
                debug    = 0;
      void load_internals() {
        exchange = args.exchange;
        base     = args.base;
        quote    = args.quote;
        version  = args.free;
        apikey   = args.apikey;
        secret   = args.secret;
        user     = args.username;
        pass     = args.passphrase;
        http     = args.http;
        ws       = args.wss;
        maxLevel = args.maxLevels;
        debug    = args.debugSecret;
        askForCancelAll = &qp.cancelOrdersAuto;
        monitor->product.minTick = &minTick;
        monitor->product.minSize = &minSize;
        if (args.latency)
          latency();
      };
      void load_externals() {
        validate(handshake());
      };
      void connect() {
        socket->connect(ws, nullptr, {}, 5e+3, &socket->getDefaultGroup<uWS::CLIENT>());
      };
      virtual void run() {
        socket->run();
      };
      virtual void end() {
        if (args.dustybot)
          print("GW " + exchange, "--dustybot is enabled, remember to cancel manually any open order.");
        else if (write_mOrder) {
          print("GW " + exchange, "Attempting to cancel all open orders, please wait.");
          for (mOrder &it : sync_cancelAll()) write_mOrder(it);
          print("GW " + exchange, "cancel all open orders OK");
        }
      };
      void quit() const {
        raise(SIGINT);
      };
    protected:
      void reconnect(const string &reason) {
        countdown = 7;
        print("GW " + exchange, "WS " + reason + ", reconnecting in " + to_string(countdown) + "s.");
      };
      void log(const string &reason) {
        const string prefix = string(
          reason.find(">>>") != reason.find("<<<")
            ? "DEBUG" : "GW"
        ) + ' ' + exchange;
        if (reason.find("Error") == string::npos)
          print(prefix, reason);
        else warn(prefix, reason);
      };
    private:
      void latency() {
        screen->printme(this);
        focus("GW " + exchange, "latency check", "start");
        const mClock Tstart = Tstamp;
        load_externals();
        const mClock Tstop  = Tstamp;
        focus("GW " + exchange, "latency check", "stop");
        const unsigned int Tdiff = Tstop - Tstart;
        focus("GW " + exchange, "HTTP read/write handshake took", to_string(
          Tdiff
        ) + "ms of your time");
        string result = "very bad; move to another server/network";
        if (Tdiff < 200) result = "very good; most trades don't enjoy such speed!";
        else if (Tdiff < 500) result = "good; most traders get the same result";
        else if (Tdiff < 700) result = "a bit bad; most trades get better results";
        else if (Tdiff < 1000) result = "bad; is possible a move to another server/network?";
        print("GW " + exchange, "This result is " + result);
        quit();
      };
      void validate(const json &reply) {
        if (!randId or symbol.empty())
          EXIT(error("GW", "Incomplete handshake aborted."));
        if (!minTick or !minSize)
          EXIT(error("GW", "Unable to fetch data from " + exchange
            + " for symbol \"" + symbol + "\", possible error message: "
            + reply.dump()));
        if (exchange != "NULL")
          print("GW " + exchange, "allows client IP");
        unsigned int precision = minTick < 1e-8 ? 10 : 8;
        print("GW " + exchange + ":", string("\n")
          + "- autoBot: " + (!args.autobot ? "no" : "yes") + '\n'
          + "- symbols: " + symbol + '\n'
          + "- minTick: " + strX(minTick, precision) + '\n'
          + "- minSize: " + strX(minSize, precision) + '\n'
          + "- makeFee: " + strX(makeFee, precision) + '\n'
          + "- takeFee: " + strX(takeFee, precision));
      };
  };

  static class Gw: public GwExchange {
    public:
//BO non-free gw library functions from build-*/local/lib/K-*.a (it just returns a derived gateway class based on argument).
/**/  static Gw* new_Gw(const string&); // may return too a nullptr instead of a child gateway class, if string is unknown..
//EO non-free gw library functions from build-*/local/lib/K-*.a (it just returns a derived gateway class based on argument).
  } *gw = nullptr;

  class GwApiREST: public Gw {
    public:
      const bool askForData(const unsigned int &tick) {
        return askForSyncData(tick);
      };
      const bool waitForData() {
        return waitForSyncData();
      };
  };
  class GwApiWS: public Gw {
    public:
      GwApiWS()
      { countdown = 1; };
      const bool askForData(const unsigned int &tick) {
        return askForNeverAsyncData(tick);
      };
      const bool waitForData() {
        return waitForNeverAsyncData();
      };
  };

  class GwNull: public GwApiREST {
    protected:
      const json handshake() {
        randId  = mRandom::uuid36Id;
        symbol  = base + "_" + quote;
        minTick = 0.01;
        minSize = 0.01;
        return nullptr;
      };
  };
  class GwHitBtc: public GwApiWS {
    protected:
      const json handshake() {
        randId = mRandom::uuid32Id;
        symbol = base + quote;
        json reply = mREST::xfer(http + "/public/symbol/" + symbol);
        minTick = stod(reply.value("tickSize", "0"));
        minSize = stod(reply.value("quantityIncrement", "0"));
        base    = reply.value("baseCurrency", base);
        quote   = reply.value("quoteCurrency", quote);
        return reply;
      };
      static json xfer(const string &url, const string &auth, const string &post) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          curl_easy_setopt(curl, CURLOPT_USERPWD, auth.data());
          curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data());
        });
      };
  };
  class GwOkCoin: public GwApiWS {
    protected:
      const json handshake() {
        randId = mRandom::char16Id;
        symbol = strL(base + "_" + quote);
        minTick = 0.0001;
        minSize = 0.001;
        return nullptr;
      };
  };
  class GwOkEx: public GwOkCoin {};
  class GwCoinbase: public GwApiWS,
                    public FIX::NullApplication {
    public:
      void run() {
        mCommand::stunnel(true);
        GwExchange::run();
      };
      void end() {
        GwExchange::end();
        mCommand::stunnel(false);
      };
    protected:
      const json handshake() {
        randId = mRandom::uuid36Id;
        symbol = base + "-" + quote;
        json reply = mREST::xfer(http + "/products/" + symbol);
        minTick = stod(reply.value("quote_increment", "0"));
        minSize = stod(reply.value("base_min_size", "0"));
        return reply;
      };
      static json xfer(const string &url, const string &h1, const string &h2, const string &h3, const string &h4, const bool &rm) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          struct curl_slist *h_ = NULL;
          h_ = curl_slist_append(h_, ("CB-ACCESS-KEY: " + h1).data());
          h_ = curl_slist_append(h_, ("CB-ACCESS-SIGN: " + h2).data());
          h_ = curl_slist_append(h_, ("CB-ACCESS-TIMESTAMP: " + h3).data());
          h_ = curl_slist_append(h_, ("CB-ACCESS-PASSPHRASE: " + h4).data());
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h_);
          if (rm) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        });
      };
  };
  class GwBitfinex: public GwApiWS {
    protected:
      const json handshake() {
        randId = mRandom::int45Id;
        symbol = strL(base + quote);
        json reply = mREST::xfer(http + "/pubticker/" + symbol);
        if (reply.find("last_price") != reply.end()) {
          ostringstream price_;
          price_ << scientific << stod(reply.value("last_price", "0"));
          string _price_ = price_.str();
          for (string::iterator it=_price_.begin(); it!=_price_.end();)
            if (*it == '+' or *it == '-') break; else it = _price_.erase(it);
          istringstream iss("1e" + to_string(fmax(stod(_price_),-4)-4));
          iss >> minTick;
        }
        reply = mREST::xfer(http + "/symbols_details");
        if (reply.is_array())
          for (json::iterator it=reply.begin(); it!=reply.end();++it)
            if (it->find("pair") != it->end() and it->value("pair", "") == symbol)
              minSize = stod(it->value("minimum_order_size", "0"));
        return reply;
      };
      static json xfer(const string &url, const string &post, const string &h1, const string &h2) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          struct curl_slist *h_ = NULL;
          h_ = curl_slist_append(h_, ("X-BFX-APIKEY: " + h1).data());
          h_ = curl_slist_append(h_, ("X-BFX-PAYLOAD: " + post).data());
          h_ = curl_slist_append(h_, ("X-BFX-SIGNATURE: " + h2).data());
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h_);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data());
        });
      };
  };
  class GwEthfinex: public GwBitfinex {};
  class GwFCoin: public GwApiWS {
    protected:
      const json handshake() {
        randId = mRandom::char16Id;
        symbol = strL(base + quote);
        json reply = mREST::xfer(http + "public/symbols");
        if (reply.find("data") != reply.end() and reply.at("data").is_array())
          for (json::iterator it=reply.at("data").begin(); it!=reply.at("data").end();++it)
            if (it->find("name") != it->end() and it->value("name", "") == symbol) {
              istringstream iss(
                "1e-" + to_string(it->value("price_decimal", 0))
                + " 1e-" + to_string(it->value("amount_decimal", 0))
              );
              iss >> minTick >> minSize;
              break;
            }
        return reply;
      };
      static json xfer(const string &url, const string &h1, const string &h2, const string &h3) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          struct curl_slist *h_ = NULL;
          h_ = curl_slist_append(h_, ("FC-ACCESS-KEY: " + h1).data());
          h_ = curl_slist_append(h_, ("FC-ACCESS-SIGNATURE: " + h2).data());
          h_ = curl_slist_append(h_, ("FC-ACCESS-TIMESTAMP: " + h3).data());
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h_);
        });
      };
      static json xfer(const string &url, const string &h1, const string &h2, const string &h3, const string &post) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          struct curl_slist *h_ = NULL;
          h_ = curl_slist_append(h_, ("FC-ACCESS-KEY: " + h1).data());
          h_ = curl_slist_append(h_, ("FC-ACCESS-SIGNATURE: " + h2).data());
          h_ = curl_slist_append(h_, ("FC-ACCESS-TIMESTAMP: " + h3).data());
          h_ = curl_slist_append(h_, "Content-Type: application/json;charset=UTF-8");
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h_);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data());
        });
      };
  };
  class GwKraken: public GwApiREST {
    protected:
      const json handshake() {
        randId = mRandom::int32Id;
        symbol = base + quote;
        json reply = mREST::xfer(http + "/0/public/AssetPairs?pair=" + symbol);
        if (reply.find("result") != reply.end())
          for (json::iterator it = reply.at("result").begin(); it != reply.at("result").end(); ++it)
            if (it.value().find("pair_decimals") != it.value().end()) {
              istringstream iss(
                "1e-" + to_string(it.value().value("pair_decimals", 0))
                + " 1e-" + to_string(it.value().value("lot_decimals", 0))
              );
              iss >> minTick >> minSize;
              symbol = it.key();
              base = it.value().value("base", base);
              quote = it.value().value("quote", quote);
              break;
            }
        return reply;
      };
      static json xfer(const string &url, const string &h1, const string &h2, const string &post) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          struct curl_slist *h_ = NULL;
          h_ = curl_slist_append(h_, ("API-Key: " + h1).data());
          h_ = curl_slist_append(h_, ("API-Sign: " + h2).data());
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h_);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data());
        });
      };
  };
  class GwKorbit: public GwApiREST {
    protected:
      const json handshake() {
        randId = mRandom::int45Id;
        symbol = strL(base + "_" + quote);
        json reply = mREST::xfer(http + "/constants");
        if (reply.find(symbol.substr(0,3).append("TickSize")) != reply.end()) {
          minTick = reply.value(symbol.substr(0,3).append("TickSize"), 0.0);
          minSize = 0.015;
        }
        return reply;
      };
      static json xfer(const string &url, const string &h1, const string &post) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          struct curl_slist *h_ = NULL;
          if (!post.empty()) {
            h_ = curl_slist_append(h_, "Content-Type: application/x-www-form-urlencoded");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data());
          }
          h_ = curl_slist_append(h_, ("Authorization: Bearer " + h1).data());
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h_);
        });
      };
  };
  class GwPoloniex: public GwApiREST {
    protected:
      const json handshake() {
        randId = mRandom::int45Id;
        symbol = quote + "_" + base;
        json reply = mREST::xfer(http + "/public?command=returnTicker");
        if (reply.find(symbol) != reply.end()) {
          istringstream iss("1e-" + to_string(6-reply.at(symbol).at("last").get<string>().find(".")));
          iss >> minTick;
          minSize = 0.001;
        }
        return reply;
      };
      static json xfer(const string &url, const string &post, const string &h1, const string &h2) {
        return mREST::curl_perform(url, [&](CURL *curl) {
          struct curl_slist *h_ = NULL;
          h_ = curl_slist_append(h_, "Content-Type: application/x-www-form-urlencoded");
          h_ = curl_slist_append(h_, ("Key: " + h1).data());
          h_ = curl_slist_append(h_, ("Sign: " + h2).data());
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h_);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data());
        });
      };
  };

  static class Engine {
#define SQLITE_BACKUP      \
        SQLITE_BACKUP_LIST \
      ( SQLITE_BACKUP_CODE )
#define SQLITE_BACKUP_CODE(data)         sqlite->backup(&data);
#define SQLITE_BACKUP_LIST(code)         \
  code( qp                             ) \
  code( wallet.target                  ) \
  code( wallet.profits                 ) \
  code( levels.stats.ewma.fairValue96h ) \
  code( levels.stats.ewma              ) \
  code( levels.stats.stdev             ) \
  code( broker.tradesHistory           )

#define SCREEN_PRINTME      \
        SCREEN_PRINTME_LIST \
      ( SCREEN_PRINTME_CODE )
#define SCREEN_PRINTME_CODE(data)     screen->printme(&data);
#define SCREEN_PRINTME_LIST(code)     \
  code( *gw                        )  \
  code( wallet.target              )  \
  code( levels.stats.fairPrice     )  \
  code( levels.stats.ewma          )  \
  code( broker.semaphore           )  \
  code( broker.calculon.nextQuotes )  \
  code( broker.calculon.dummyMM    )  \
  code( broker.tradesHistory       )  \
  code( broker                     )

#define SCREEN_PRESSME      \
        SCREEN_PRESSME_LIST \
      ( SCREEN_PRESSME_CODE )
#define SCREEN_PRESSME_CODE(key, fn)    screen->pressme(mHotkey::key, [&]() { fn(); });
#define SCREEN_PRESSME_LIST(code)       \
  code(  Q  , gw->quit                ) \
  code(  q  , gw->quit                ) \
  code( ESC , broker.semaphore.toggle )

#define CLIENT_WELCOME      \
        CLIENT_WELCOME_LIST \
      ( CLIENT_WELCOME_CODE )
#define CLIENT_WELCOME_CODE(data)  client->welcome(data);
#define CLIENT_WELCOME_LIST(code)  \
  code( qp                       ) \
  code( notepad                  ) \
  code( monitor                  ) \
  code( monitor.product          ) \
  code( wallet.target.safety     ) \
  code( wallet.target            ) \
  code( wallet                   ) \
  code( levels.diff              ) \
  code( levels.stats.takerTrades ) \
  code( levels.stats.fairPrice   ) \
  code( levels.stats             ) \
  code( broker.semaphore         ) \
  code( broker.calculon          ) \
  code( broker.tradesHistory     ) \
  code( broker                   )

#define CLIENT_CLICKME      \
        CLIENT_CLICKME_LIST \
      ( CLIENT_CLICKME_CODE )
#define CLIENT_CLICKME_CODE(btn, fn, val) \
                  client->clickme(btn, [&](const json &butterfly) { fn(val); });
#define CLIENT_CLICKME_LIST(code)                                              \
  code( qp                    , savedQuotingParameters           ,           ) \
  code( notepad               , void                             ,           ) \
  code( broker.semaphore      , void                             ,           ) \
  code( btn.submit            , manualSendOrder                  , butterfly ) \
  code( btn.cancel            , manualCancelOrder                , butterfly ) \
  code( btn.cancelAll         , cancelOrders                     ,           ) \
  code( btn.cleanTrade        , broker.tradesHistory.clearOne    , butterfly ) \
  code( btn.cleanTradesClosed , broker.tradesHistory.clearClosed ,           ) \
  code( btn.cleanTrades       , broker.tradesHistory.clearAll    ,           )
    public:
      mWalletPosition wallet;
        mMarketLevels levels;
              mBroker broker;
             mButtons btn;
             mNotepad notepad;
             mMonitor monitor;
      Engine()
        : levels(&monitor.product, &broker.orders)
        , broker(&monitor.product, &wallet, &levels)
      {};
      void savedQuotingParameters() {
        broker.calculon.dummyMM.reset("saved");
        levels.stats.ewma.calcFromHistory();
      };
      void timer_1s(const unsigned int &tick) {                     PRETTY_DEBUG
        if (levels.warn_empty()) return;
        levels.timer_1s();
        if (!(tick % 60)) {
          levels.timer_60s();
          monitor.timer_60s();
        }
        wallet.target.safety.calc(levels, broker.tradesHistory);
        calcQuotes();
      };
      void calcQuotes() {                                           PRETTY_DEBUG
        broker.calculon.reset();
        if (!broker.semaphore.greenGateway) {
          broker.calculon.reset(mQuoteState::Disconnected);
        } else if (levels.filter() and !wallet.target.safety.empty()) {
          if (!broker.semaphore.greenButton) {
            broker.calculon.reset(mQuoteState::DisabledQuotes);
            cancelOrders();
          } else {
            broker.calculon.reset(mQuoteState::UnknownHeld);
            broker.calculon.calcQuotes();
            quote2orders(broker.calculon.nextQuotes.ask, mSide::Ask);
            quote2orders(broker.calculon.nextQuotes.bid, mSide::Bid);
          }
        }
        broker.calculon.send();
      };
      void quote2orders(const mQuote &nextQuote, const mSide &side) {
        if (nextQuote.state != mQuoteState::Live)
          return cancelOrders(side);
        unsigned int n = 0;
        bool skipNextQuote = false;
        vector<mOrder*> toCancel,
                        keepWorking;
        vector<mRandId> zombies;
        mClock now = Tstamp;
        for (unordered_map<mRandId, mOrder>::value_type &it : broker.orders)
          if (it.second.side != side) continue;
          else {
            if (it.second.orderStatus == mStatus::Waiting) {
              if (now - 10e+3 > it.second.time) {
                zombies.push_back(it.first);
                continue;
              }
              ++broker.calculon.countWaiting;
            } else if (it.second.orderStatus == mStatus::Working)
              ++broker.calculon.countWorking;
            if (!it.second.preferPostOnly) continue;
            if (abs(it.second.price - nextQuote.price) < *monitor.product.minTick) skipNextQuote = true;
            else if (it.second.orderStatus == mStatus::Waiting) {
              if (qp.safety != mQuotingSafety::AK47 or ++n >= qp.bullets) skipNextQuote = true;
            } else if (qp.safety != mQuotingSafety::AK47 or (
              mSide::Bid == side
                ? nextQuote.price <= it.second.price
                : nextQuote.price >= it.second.price
            )) {
              if (args.lifetime and it.second.time + args.lifetime > now) skipNextQuote = true;
              else toCancel.push_back(&it.second);
            } else keepWorking.push_back(&it.second);
          }
        for (mRandId &it : zombies) broker.erase(it);
        if (qp.safety == mQuotingSafety::AK47
          and toCancel.empty()
          and !keepWorking.empty()
        ) toCancel.push_back(keepWorking.back());
        sendOrders(toCancel, skipNextQuote ? nullptr : &nextQuote, side);
      };
      void sendOrders(vector<mOrder*> toCancel, const mQuote *const nextQuote, const mSide &side) {
        mOrder *toReplace = nullptr;
        if (nextQuote and !toCancel.empty()) {
          toReplace = toCancel.back();
          toCancel.pop_back();
        }
        for (mOrder *const it : toCancel)
          cancelOrder(it);
        if (!nextQuote) return;
        if (toReplace and gw->replace)
          replaceOrder(toReplace, nextQuote->price, nextQuote->isPong);
        else {
          if (toReplace and args.testChamber != 1) cancelOrder(toReplace);
          placeOrder(mOrder(
            gw->randId(), side, nextQuote->price, nextQuote->size, mOrderType::Limit, nextQuote->isPong, mTimeInForce::GTC
          ));
          if (toReplace and args.testChamber == 1) cancelOrder(toReplace);
        }
        monitor.tick_orders();
      };
      void cancelOrders(const mSide &side = mSide::Both) {
        for (mOrder *const it : broker.working(side))
          cancelOrder(it);
      };
      void manualSendOrder(mOrder order) {
        order.orderId = gw->randId();
        placeOrder(order);
      };
      void manualCancelOrder(const mRandId &orderId) {
        cancelOrder(broker.find(orderId));
      };
    private:
      void placeOrder(const mOrder &order) {
        gw->place(broker.upsert(order));
      };
      void replaceOrder(mOrder *const toReplace, const mPrice &price, const bool &isPong) {
        if (broker.replace(toReplace, price, isPong))
          gw->replace(toReplace->exchangeId, str8(toReplace->price));
      };
      void cancelOrder(mOrder *const order) {
        if (broker.cancel(order))
          gw->cancel(order->orderId, order->exchangeId);
      };
  } *engine = nullptr;

  static string tracelog;
  static vector<function<void()>> happyEndingFn, endingFn = { []() {
    screen->end();
    cout << (args.latency ? "" : "\n") << screen->stamp() << tracelog;
  } };
  static class Ending {
    public:
      Ending(/* KMxTWEpb9ig */) {
        tracelog = "- roll-out: " + to_string(Tstamp) + '\n';
        signal(SIGINT, quit);
        signal(SIGABRT, wtf);
        signal(SIGSEGV, wtf);
#ifndef _WIN32
        signal(SIGUSR1, wtf);
#endif
      };
    private:
      static void halt(const int code) {
        endingFn.swap(happyEndingFn);
        for (function<void()> &it : happyEndingFn) it();
        if (code == EXIT_FAILURE)
          this_thread::sleep_for(chrono::seconds(3));
        cout << BGREEN << 'K'
             << RGREEN << " exit code "
             << BGREEN << to_string(code)
             << RGREEN << '.'
             << RRESET << '\n';
        EXIT(code);
      };
      static void quit(const int sig) {
        tracelog = args.latency
          ? "1 HTTP connection done"
            + string(RWHITE) + " (consider to repeat a few times this check).\n"
          : "Excellent decision! "
            + mREST::xfer("https://api.icndb.com/jokes/random?escape=javascript&limitTo=[nerdy]", 4L)
                .value("/value/joke"_json_pointer, "let's plant a tree instead..")
            + '\n';
        halt(EXIT_SUCCESS);
      };
      static void wtf(const int sig) {
        const string rollout = tracelog;
        tracelog = string(RCYAN) + "Errrror: Signal " + to_string(sig) + ' '
#ifndef _WIN32
          + strsignal(sig)
#endif
          + ' ';
        if (mCommand::deprecated())
          tracelog += string("(deprecated K version found).") + '\n'
            + '\n' + string(BYELLOW) + "Hint!" + string(RYELLOW)
            + '\n' + "please upgrade to the latest commit; the encountered error may be already fixed at:"
            + '\n' + mCommand::changelog()
            + '\n' + "If you agree, consider to run \"make latest\" prior further executions."
            + '\n' + '\n';
        else {
          tracelog += string("(Three-Headed Monkey found):") + '\n'
            + "- exchange: " + args.exchange + '\n'
            + "- currency: " + args.currency + '\n'
            + rollout
            + "- lastbeat: " + to_string(Tstamp) + '\n'
            + "- binbuild: " + string(K_BUILD) + '\n'
#ifndef _WIN32
            + "- os-uname: " + mCommand::uname()
            + "- tracelog: " + '\n';
          void *k[69];
          size_t jumps = backtrace(k, 69);
          char **trace = backtrace_symbols(k, jumps);
          size_t i;
          for (i = 0; i < jumps; i++)
            tracelog += string(trace[i]) + '\n';
          free(trace)
#endif
          ;
          tracelog += '\n' + string(BRED) + "Yikes!" + string(RRED)
            + '\n' + "please copy and paste the error above into a new github issue (noworry for duplicates)."
            + '\n' + "If you agree, go to https://github.com/ctubio/Krypto-trading-bot/issues/new"
            + '\n' + '\n';
        }
        halt(EXIT_FAILURE);
      };
  } ending;

  class Klass {
    protected:
      virtual void load() {};
      virtual void waitData() {};
      virtual void waitTime() {};
      virtual void waitWebAdmin() {};
      virtual void waitSysAdmin() {};
      virtual void run() {};
      virtual void end() {};
    public:
      void wait() {
        load();
        waitData();
        if (!args.headless)
          waitWebAdmin();
        waitSysAdmin();
        waitTime();
        run();
        endingFn.push_back([&](){
          end();
        });
        if (gw->ready()) gw->run();
      };
  };
}

#endif