#include <pjsua2.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <optionparser.h>
#include <pj/file_access.h>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

struct rk_sema {
#ifdef __APPLE__
    dispatch_semaphore_t    sem;
#else
    sem_t                   sem;
#endif
};

    static inline void
rk_sema_init(struct rk_sema *s, uint32_t value)
{
#ifdef __APPLE__
    dispatch_semaphore_t *sem = &s->sem;

    *sem = dispatch_semaphore_create(value);
#else
    sem_init(&s->sem, 0, value);
#endif
}

    static inline void
rk_sema_wait(struct rk_sema *s)
{

#ifdef __APPLE__
    dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
#else
    int r;

    do {
        r = sem_wait(&s->sem);
    } while (r == -1 && errno == EINTR);
#endif
}

    static inline void
rk_sema_post(struct rk_sema *s)
{

#ifdef __APPLE__
    dispatch_semaphore_signal(s->sem);
#else
    sem_post(&s->sem);
#endif
}

struct Arg: public option::Arg
{

    static void printError(const char* msg1, const option::Option& opt, const char* msg2)
    {
        fprintf(stderr, "%s", msg1);
        fwrite(opt.name, opt.namelen, 1, stderr);
        fprintf(stderr, "%s", msg2);
    }


    static option::ArgStatus Required(const option::Option& option, bool msg)
    {
        if (option.arg != 0)
            return option::ARG_OK;

        if (msg) printError("Option '", option, "' requires an argument\n");
        return option::ARG_ILLEGAL;
    }
    static option::ArgStatus Numeric(const option::Option& option, bool msg)
    {
        char* endptr = 0;
        if (option.arg != 0 && strtol(option.arg, &endptr, 10)){};
        if (endptr != option.arg && *endptr == 0)
            return option::ARG_OK;

        if (msg) printError("Option '", option, "' requires a numeric argument\n");
        return option::ARG_ILLEGAL;
    }

};



enum  optionIndex { UNKNOWN, HELP, SOUND, ID, REG_URI, PROXY, REALM, USER, PASSWORD, DST_URI, DEBUG };
const option::Descriptor usage[] =
{
    {UNKNOWN, 0, "", "",option::Arg::None, "USAGE: caller [options]\n\n"
        "Options:" },
    {HELP, 0,"", "help",option::Arg::None, "  --help  \tPrint usage and exit." },
    {SOUND, 0,"s","sound", Arg::Required, "  --sound=wav-file, -s wav-file   \tThe message to play." },
    {ID, 0,"i","id", Arg::Required, "  --id=id, -i id   \tThe SIP is used to originate the call from." },
    {REG_URI, 0,"r","reg_uri", Arg::Required, "  --reg_uri=uri, -r reg_uri   \tThe SIP URI where to register." },
    {PROXY, 0,"","proxy", Arg::Required, "  --proxy=uri   \tThe SIP URI of the proxy." },
    {REALM, 0,"","realm", Arg::Required, "  --realm=realm_name   \tThe authentication realm." },
    {USER, 0,"u","user", Arg::Required, "  --user=user_id, -u user_id   \tThe SIP user to authenticate with." },
    {PASSWORD, 0,"p","pw", Arg::Required, "  --pw=password, -p password   \tThe password to authenticate with." },
    {DST_URI, 0,"d","dst_uri", Arg::Required, "  --dst_uri=sip, -d sip   \tThe SIP uri to call." },
    {DEBUG, 0,"","loglevel", Arg::Numeric, "  --loglevel=1   \tThe log level for debugging." },
    {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
        "  caller -s test.wav -i sip:120@sipserver -r sip:sipserver \\\n\t--proxy=sip:sipserver --realm=* -u 120 \\\n\t-p test -d sip:103@sipserver\n"
    },
    {0,0,0,0,0,0}
};



using namespace pj;

class MyAccount;

class MyPlayer : AudioMediaPlayer
{
    private:
        rk_sema sem;
    public:
        MyPlayer(const char *wave_file)
        {
            rk_sema_init(&sem, 0);
            createPlayer(wave_file, PJMEDIA_FILE_NO_LOOP);
        }

        void start_playback(const AudioMedia &sink)
        {
            startTransmit(sink);
            std::cout << "*** Started Audio playback" << std::endl;
            rk_sema_wait(&sem);
        }

        void onEof2()
        {
            std::cout << "*** Audio has finished" << std::endl;
            rk_sema_post(&sem);
        }

};

class MyCall : public Call
{
    private:
        MyAccount *myAcc;
        MyPlayer *wav_player;
        rk_sema sem;

    public:
        MyCall(Account &acc, MyPlayer *wp, int call_id = PJSUA_INVALID_ID) : Call(acc, call_id)
    {
        wav_player = wp;
        myAcc = (MyAccount *)&acc;
        rk_sema_init(&sem,0);
    }

        void wait()
        {
            rk_sema_wait(&sem);
        }

        ~MyCall()
        {
            if(wav_player)
                delete wav_player;
        }

        virtual void onCallState(OnCallStateParam &prm);
        virtual void onCallMediaState(OnCallMediaStateParam &prm);
        virtual pjsip_redirect_op onCallRedirected(OnCallRedirectedParam &prm);

};

// Subclass to extend the Account and get notifications etc.
class MyAccount : public Account
{
    private:
        rk_sema sem;

    public:
        MyAccount()
        {
            rk_sema_init(&sem, 0);
        }
        void wait()
        {
            rk_sema_wait(&sem);
        }
        virtual void onRegState(OnRegStateParam &prm) {
            AccountInfo ai = getInfo();
            std::cout << (ai.regIsActive? "*** Register:" : "*** Unregister:")
                << " code=" << prm.code << std::endl;
            rk_sema_post(&sem);
        }
};

void MyCall::onCallState(OnCallStateParam &prm)
{
    PJ_UNUSED_ARG(prm);

    CallInfo ci = getInfo();
    std::cout << "*** Call: " << ci.remoteUri << " [ " << ci.stateText << " ] " << std::endl;

    if(ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        rk_sema_post(&sem);
    }
}

void MyCall::onCallMediaState(OnCallMediaStateParam &prm)
{
    PJ_UNUSED_ARG(prm);

    CallInfo ci = getInfo();


    AudioMedia aud_med;
    /* AudioMedia& play_dev_med = */
    /*     Endpoint::instance().audDevManager().getPlaybackDevMedia(); */

    try {
        // Get the first audio media
        aud_med = getAudioMedia(-1);
    } catch(...) {
        std::cout << "Failed to get audio media" << std::endl;
        return;
    }

    if(wav_player) {
        wav_player->start_playback(aud_med);
        rk_sema_post(&sem);
    }

    /* aud_med.startTransmit(play_dev_med); */
}

pjsip_redirect_op MyCall::onCallRedirected(OnCallRedirectedParam &prm)
{
    PJ_UNUSED_ARG(prm);

    return PJSIP_REDIRECT_ACCEPT;
}



int main(int argc, char* argv[])
{
    argc-=(argc>0); argv+=(argc>0); // skip program name argv[0] if present
    option::Stats  stats(usage, argc, argv);
    std::vector<option::Option> options(stats.options_max);
    std::vector<option::Option> buffer(stats.buffer_max);
    option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

    if (parse.error())
        return 1;

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    const char *sound=NULL;
    const char *id=NULL;
    const char *reg_uri=NULL;
    const char *realm="*";
    const char *user=NULL;
    const char *password=NULL;
    const char *dst_uri=NULL;

    if (options[SOUND]) {
        sound = options[SOUND].last()->arg;
    }
    else {
        option::printUsage(std::cout, usage);
        return 1;
    }
    if (options[ID]) {
        id = options[ID].last()->arg;
    }
    else {
        option::printUsage(std::cout, usage);
        return 1;
    }
    if (options[REG_URI]) {
        reg_uri = options[REG_URI].last()->arg;
    }
    else {
        option::printUsage(std::cout, usage);
        return 1;
    }
    if (options[REALM]) {
        realm = options[REALM].last()->arg;
    }
    if (options[USER]) {
        user = options[USER].last()->arg;
    }
    else {
        option::printUsage(std::cout, usage);
        return 1;
    }
    if (options[PASSWORD]) {
        password = options[PASSWORD].last()->arg;
    }
    else {
        option::printUsage(std::cout, usage);
        return 1;
    }
    if (options[DST_URI]) {
        dst_uri = options[DST_URI].last()->arg;
    }
    else {
        option::printUsage(std::cout, usage);
        return 1;
    }


    Endpoint ep;

    ep.libCreate();

    // Initialize endpoint
    EpConfig ep_cfg;

    if(options[DEBUG]) {
        long int i;
        char* pEnd;
        i = strtol(options[DEBUG].last()->arg, &pEnd, 10);

        ep_cfg.logConfig.level = i;

    } else
        ep_cfg.logConfig.level = 0;

    ep.libInit( ep_cfg );


    // Create SIP transport. Error handling sample is shown
    TransportConfig tcfg;
    tcfg.port = 0;
    try {
        ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
    } catch (Error &err) {
        std::cout << err.info() << std::endl;
        return 1;
    }

    // Start the library (worker threads etc)
    ep.libStart();
    // set NULL audio device
    ep.audDevManager().setNullDev();

    // Check validity of given WAV file.
    std::cout << "Checking wav file '" << sound << "'" << std::endl;

    MyPlayer *wav_player;
    try {
        wav_player = new MyPlayer(sound);
    } catch (Error e) {
        std::cout << "Failed opening wav file" << std::endl;
        std::cout << "Reason: " << e.reason << std::endl;
        delete wav_player;
        return 1;
    }


    // Configure an AccountConfig
    AccountConfig acfg;
    acfg.idUri = id;
    acfg.regConfig.registrarUri = reg_uri;
    if (options[PROXY]) {
        std::string proxy = options[PROXY].last()->arg;
        StringVector v(1);
        v[1] = proxy;
        acfg.sipConfig.proxies = v;
    }
    AuthCredInfo cred("digest", realm, user, 0, password);
    acfg.sipConfig.authCreds.push_back( cred );

    // Create the account
    MyAccount *acc = new MyAccount;
    acc->create(acfg);

    // Here we don't have anything else to do..
    acc->wait();

    MyCall *call = new MyCall(*acc, wav_player);

    CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    call->makeCall(dst_uri, prm);
    call->wait();

    /* pj_thread_sleep(4000); */
    ep.hangupAllCalls();
    pj_thread_sleep(4000);

    // Delete the account. This will unregister from server
    delete acc;

    // This will implicitly shutdown the library
    return 0;
}
