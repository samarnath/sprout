/**
 * @file scscf_test.cpp UT for S-CSCF functionality
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <boost/lexical_cast.hpp>

#include "pjutils.h"
#include "constants.h"
#include "siptest.hpp"
#include "utils.h"
#include "test_utils.hpp"
#include "analyticslogger.h"
#include "fakecurl.hpp"
#include "fakehssconnection.hpp"
#include "fakexdmconnection.hpp"
#include "test_interposer.hpp"
#include "fakechronosconnection.hpp"
#include "scscfsproutlet.h"
#include "icscfsproutlet.h"
#include "bgcfsproutlet.h"
#include "sproutletappserver.h"
#include "scscfselector.h"
#include "mmtel.h"
#include "sproutletproxy.h"
#include "fakesnmp.hpp"
#include "mock_as_communication_tracker.h"

using namespace std;
using testing::StrEq;
using testing::ElementsAre;
using testing::MatchesRegex;
using testing::HasSubstr;
using testing::Not;
using testing::_;
using testing::NiceMock;
using testing::HasSubstr;
using ::testing::Return;

const std::string IMS_SUB_BARRED_MULTIPLE_WILDCARD =
                               "<IMSSubscription>\n"
                               "  <ServiceProfile>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:610@homedomain</Identity>"
                               "    </PublicIdentity>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:6!.*!@homedomain</Identity>"
                               "    </PublicIdentity>\n"
                               "    <InitialFilterCriteria>\n"
                               "      <Priority>1</Priority>\n"
                               "      <ApplicationServer>\n"
                               "        <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                               "        <DefaultHandling>0</DefaultHandling>\n"
                               "      </ApplicationServer>\n"
                               "    </InitialFilterCriteria>\n"
                               "  </ServiceProfile>\n"
                               "  <ServiceProfile>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:611@homedomain</Identity>"
                               "    </PublicIdentity>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:65!.*!@homedomain</Identity>"
                               "    </PublicIdentity>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:650!.*!@homedomain</Identity>"
                               "    </PublicIdentity>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:6505551000@homedomain</Identity>"
                               "      <BarringIndication>1</BarringIndication>"
                               "      <Extension>"
                               "        <IdentityType>3</IdentityType>"
                               "        <Extension>"
                               "          <Extension>"
                               "            <WildcardedIMPU>sip:65!.*!@homedomain</WildcardedIMPU>"
                               "          </Extension>"
                               "        </Extension>"
                               "      </Extension>"
                               "    </PublicIdentity>\n"
                               "    <InitialFilterCriteria>\n"
                               "      <Priority>1</Priority>\n"
                               "      <ApplicationServer>\n"
                               "        <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                               "        <DefaultHandling>0</DefaultHandling>\n"
                               "      </ApplicationServer>\n"
                               "    </InitialFilterCriteria>\n"
                               "  </ServiceProfile>\n"
                               "  <ServiceProfile>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:612@homedomain</Identity>"
                               "    </PublicIdentity>\n"
                               "    <PublicIdentity>"
                               "      <Identity>sip:!.*!@homedomain</Identity>"
                               "    </PublicIdentity>\n"
                               "    <InitialFilterCriteria>\n"
                               "      <Priority>1</Priority>\n"
                               "      <ApplicationServer>\n"
                               "        <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                               "        <DefaultHandling>0</DefaultHandling>\n"
                               "      </ApplicationServer>\n"
                               "    </InitialFilterCriteria>\n"
                               "  </ServiceProfile>\n"
                               "</IMSSubscription>";
const std::string IMS_SUB_BARRED_WILDCARD =
                               "<IMSSubscription><ServiceProfile>\n"
                               "  <PublicIdentity>"
                               "    <Identity>sip:610@homedomain</Identity>"
                               "  </PublicIdentity>\n"
                               "  <PublicIdentity>"
                               "    <Identity>sip:65!.*!@homedomain</Identity>"
                               "    <BarringIndication>1</BarringIndication>"
                               "  </PublicIdentity>\n"
                               "  <InitialFilterCriteria>\n"
                               "    <Priority>1</Priority>\n"
                               "    <TriggerPoint>\n"
                               "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                               "    <SPT>\n"
                               "      <ConditionNegated>1</ConditionNegated>\n"
                               "      <Group>0</Group>\n"
                               "      <Method>INVITE</Method>\n"
                               "      <Extension></Extension>\n"
                               "    </SPT>\n"
                               "  </TriggerPoint>\n"
                               "  <ApplicationServer>\n"
                               "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                               "    <DefaultHandling>0</DefaultHandling>\n"
                               "  </ApplicationServer>\n"
                               "  </InitialFilterCriteria>\n"
                               "</ServiceProfile></IMSSubscription>";
const std::string IMS_SUB_BARRED_IMPU_IN_WILDCARD =
                               "<IMSSubscription><ServiceProfile>\n"
                               "  <PublicIdentity>"
                               "    <Identity>sip:610@homedomain</Identity>"
                               "  </PublicIdentity>\n"
                               "  <PublicIdentity>"
                               "    <Identity>sip:65!.*!@homedomain</Identity>"
                               "  </PublicIdentity>\n"
                               "  <PublicIdentity>"
                               "    <Identity>sip:6505551000@homedomain</Identity>"
                               "    <BarringIndication>1</BarringIndication>"
                               "    <Extension>"
                               "      <IdentityType>3</IdentityType>"
                               "      <Extension>"
                               "        <Extension>"
                               "          <WildcardedIMPU>sip:65!.*!@homedomain</WildcardedIMPU>"
                               "        </Extension>"
                               "      </Extension>"
                               "    </Extension>"
                               "  </PublicIdentity>\n"
                               "  <InitialFilterCriteria>\n"
                               "    <Priority>1</Priority>\n"
                               "    <TriggerPoint>\n"
                               "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                               "    <SPT>\n"
                               "      <ConditionNegated>1</ConditionNegated>\n"
                               "      <Group>0</Group>\n"
                               "      <Method>INVITE</Method>\n"
                               "      <Extension></Extension>\n"
                               "    </SPT>\n"
                               "  </TriggerPoint>\n"
                               "  <ApplicationServer>\n"
                               "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                               "    <DefaultHandling>0</DefaultHandling>\n"
                               "  </ApplicationServer>\n"
                               "  </InitialFilterCriteria>\n"
                               "</ServiceProfile></IMSSubscription>";

namespace SP
{
  class Message
  {
  public:
    string _method;
    string _requri; //< overrides toscheme:to@todomain
    string _toscheme;
    string _status;
    string _from;
    string _fromdomain;
    string _to;
    string _todomain;
    string _content_type;
    string _body;
    string _extra;
    int _forwards;
    int _unique; //< unique to this dialog; inserted into Call-ID
    bool _first_hop;
    string _via;
    string _branch;
    string _route;
    int _cseq;
    bool _in_dialog;

    Message() :
      _method("INVITE"),
      _toscheme("sip"),
      _status("200 OK"),
      _from("6505551000"),
      _fromdomain("homedomain"),
      _to("6505551234"),
      _todomain("homedomain"),
      _content_type("application/sdp"),
      _forwards(68),
      _first_hop(false),
      _via("10.83.18.38:36530"),
      _branch(""),
      _route("Route: <sip:sprout.homedomain;service=scscf>"),
      _cseq(16567),
      _in_dialog(false)
    {
      static int unique = 1042;
      _unique = unique;
      unique += 10; // leave room for manual increments
    }

    void set_route(pjsip_msg* msg)
    {
      string route = get_headers(msg, "Record-Route");
      if (route != "")
      {
        // Convert to a Route set by replacing all instances of Record-Route: with Route:
        for (size_t n = 0; (n = route.find("Record-Route:", n)) != string::npos;)
        {
          route.replace(n, 13, "Route:");
        }
      }
      _route = route;
    }

    string get_request()
    {
      char buf[16384];

      // The remote target.
      string target = string(_toscheme).append(":").append(_to);
      if (!_todomain.empty())
      {
        target.append("@").append(_todomain);
      }

      // If there's no route, the target goes in the request
      // URI. Otherwise it goes in the Route:, and the route goes in the
      // request URI.
      //string requri = _route.empty() ? target : _route;
      //string route = _route.empty() ? "" : string("Route: ").append(target).append("\r\n");
      string requri = target;
      string route = _route;
      route = route.empty() ? "" : route.append("\r\n");

      // Default branch parameter if it's not supplied.
      std::string branch = _branch.empty() ? "Pjmo1aimuq33BAI4rjhgQgBr4sY" + std::to_string(_unique) : _branch;

      int n = snprintf(buf, sizeof(buf),
                       "%1$s %9$s SIP/2.0\r\n"
                       "Via: SIP/2.0/TCP %13$s;rport;branch=z9hG4bK%16$s\r\n"
                       "%12$s"
                       "From: <sip:%2$s@%3$s>;tag=10.114.61.213+1+8c8b232a+5fb751cf\r\n"
                       "To: <%10$s>%17$s\r\n"
                       "Max-Forwards: %8$d\r\n"
                       "Call-ID: 0gQAAC8WAAACBAAALxYAAAL8P3UbW8l4mT8YBkKGRKc5SOHaJ1gMRqs%11$04dohntC@10.114.61.213\r\n"
                       "CSeq: %15$d %1$s\r\n"
                       "User-Agent: Accession 2.0.0.0\r\n"
                       "Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, SUBSCRIBE, NOTIFY, REFER, MESSAGE, OPTIONS\r\n"
                       "%4$s"
                       "%7$s"
                       "%14$s"
                       "Content-Length: %5$d\r\n"
                       "\r\n"
                       "%6$s",
                       /*  1 */ _method.c_str(),
                       /*  2 */ _from.c_str(),
                       /*  3 */ _fromdomain.c_str(),
                       /*  4 */ _content_type.empty() ? "" : string("Content-Type: ").append(_content_type).append("\r\n").c_str(),
                       /*  5 */ (int)_body.length(),
                       /*  6 */ _body.c_str(),
                       /*  7 */ _extra.empty() ? "" : string(_extra).append("\r\n").c_str(),
                       /*  8 */ _forwards,
                       /*  9 */ _requri.empty() ? requri.c_str() : _requri.c_str(),
                       /* 10 */ target.c_str(),
                       /* 11 */ _unique,
                       /* 12 */ _first_hop ? "" : "Via: SIP/2.0/TCP 10.114.61.213:5061;received=23.20.193.43;branch=z9hG4bK+7f6b263a983ef39b0bbda2135ee454871+sip+1+a64de9f6\r\n",
                       /* 13 */ _via.c_str(),
                       /* 14 */ route.c_str(),
                       /* 15 */ _cseq,
                       /* 16 */ branch.c_str(),
                       /* 17 */ (_in_dialog) ? ";tag=10.114.61.213+1+8c8b232a+5fb751cf" : ""
        );

      EXPECT_LT(n, (int)sizeof(buf));

      string ret(buf, n);
      // cout << ret <<endl;
      return ret;
    }

    string get_response()
    {
      char buf[16384];

      // Default branch parameter if it's not supplied.
      std::string branch = _branch.empty() ? "Pjmo1aimuq33BAI4rjhgQgBr4sY" + std::to_string(_unique) : _branch;

      int n = snprintf(buf, sizeof(buf),
                       "SIP/2.0 %9$s\r\n"
                       "Via: SIP/2.0/TCP %14$s;rport;branch=z9hG4bK%15$s\r\n"
                       "%12$s"
                       "From: <sip:%2$s@%3$s>;tag=10.114.61.213+1+8c8b232a+5fb751cf\r\n"
                       "To: <sip:%7$s%8$s>\r\n"
                       "Call-ID: 0gQAAC8WAAACBAAALxYAAAL8P3UbW8l4mT8YBkKGRKc5SOHaJ1gMRqs%11$04dohntC@10.114.61.213\r\n"
                       "CSeq: %13$d %1$s\r\n"
                       "User-Agent: Accession 2.0.0.0\r\n"
                       "Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, SUBSCRIBE, NOTIFY, REFER, MESSAGE, OPTIONS\r\n"
                       "%4$s"
                       "%10$s"
                       "Content-Length: %5$d\r\n"
                       "\r\n"
                       "%6$s",
                       /*  1 */ _method.c_str(),
                       /*  2 */ _from.c_str(),
                       /*  3 */ _fromdomain.c_str(),
                       /*  4 */ _content_type.empty() ? "" : string("Content-Type: ").append(_content_type).append("\r\n").c_str(),
                       /*  5 */ (int)_body.length(),
                       /*  6 */ _body.c_str(),
                       /*  7 */ _to.c_str(),
                       /*  8 */ _todomain.empty() ? "" : string("@").append(_todomain).c_str(),
                       /*  9 */ _status.c_str(),
                       /* 10 */ _extra.empty() ? "" : string(_extra).append("\r\n").c_str(),
                       /* 11 */ _unique,
                       /* 12 */ _first_hop ? "" : "Via: SIP/2.0/TCP 10.114.61.213:5061;received=23.20.193.43;branch=z9hG4bK+7f6b263a983ef39b0bbda2135ee454871+sip+1+a64de9f6\r\n",
                       /* 13 */ _cseq,
                       /* 14 */ _via.c_str(),
                       /* 15 */ branch.c_str()
        );

      EXPECT_LT(n, (int)sizeof(buf));

      string ret(buf, n);
      // cout << ret <<endl;
      return ret;
    }
  };
}

/// ABC for fixtures for SCSCFTest and friends.
class SCSCFTest : public SipTest
{
public:
  /// TX data for testing.  Will be cleaned up.  Each message in a
  /// forked flow has its URI stored in _uris, and its txdata stored
  /// in _tdata against that URI.
  vector<string> _uris;
  map<string,pjsip_tx_data*> _tdata;

  /// Set up test case.  Caller must clear host_mapping.
  static void SetUpTestCase()
  {
    SipTest::SetUpTestCase();

    _chronos_connection = new FakeChronosConnection();
    _local_data_store = new LocalStore();
    _sdm = new SubscriberDataManager((Store*)_local_data_store, _chronos_connection, true);
    _analytics = new AnalyticsLogger();
    _bgcf_service = new BgcfService(string(UT_DIR).append("/test_stateful_proxy_bgcf.json"));
    _xdm_connection = new FakeXDMConnection();
    _sess_term_comm_tracker = new NiceMock<MockAsCommunicationTracker>();
    _sess_cont_comm_tracker = new NiceMock<MockAsCommunicationTracker>();

    // We only test with a JSONEnumService, not with a DNSEnumService - since
    // it is stateful_proxy.cpp that's under test here, the EnumService
    // implementation doesn't matter.
    _enum_service = new JSONEnumService(string(UT_DIR).append("/test_stateful_proxy_enum.json"));

    _acr_factory = new ACRFactory();
    _mmf_service = new MMFService(NULL, string(UT_DIR).append("/test_mmf_targets.json"));
    _fifc_service = new FIFCService(NULL, string(UT_DIR).append("/test_scscf_fifc.xml"));

    // Schedule timers.
    SipTest::poll();
  }

  static void TearDownTestCase()
  {
    // Shut down the transaction module first, before we destroy the
    // objects that might handle any callbacks!
    pjsip_tsx_layer_destroy();
    delete _mmf_service; _mmf_service = NULL;
    delete _fifc_service; _fifc_service = NULL;
    delete _sdm; _sdm = NULL;
    delete _chronos_connection; _chronos_connection = NULL;
    delete _local_data_store; _local_data_store = NULL;
    delete _analytics; _analytics = NULL;
    delete _enum_service; _enum_service = NULL;
    delete _bgcf_service; _bgcf_service = NULL;
    delete _xdm_connection; _xdm_connection = NULL;
    delete _sess_cont_comm_tracker; _sess_cont_comm_tracker = NULL;
    delete _sess_term_comm_tracker; _sess_term_comm_tracker = NULL;
    SipTest::TearDownTestCase();
  }

  SCSCFTest()
  {
    _log_traffic = PrintingTestLogger::DEFAULT.isPrinting(); // true to see all traffic
    _local_data_store->flush_all();  // start from a clean slate on each test

    _hss_connection_observer = new MockHSSConnection();
    _hss_connection = new FakeHSSConnection(_hss_connection_observer);

    // Create the S-CSCF Sproutlet.
    IFCConfiguration ifc_configuration(false, false, "sip:DUMMY_AS", NULL, NULL);
    _scscf_sproutlet = new SCSCFSproutlet("scscf",
                                          "scscf",
                                          "sip:scscf.sprout.homedomain:5058;transport=TCP",
                                          "sip:127.0.0.1:5058",
                                          "sip:icscf.sprout.homedomain:5059;transport=TCP",
                                          "sip:bgcf@homedomain:5058",
                                          "sip:11.22.33.44:5053;transport=tcp",
                                          "sip:44.33.22.11:5053;transport=tcp",
                                          5058,
                                          "sip:scscf.sprout.homedomain:5058;transport=TCP",
                                          _sdm,
                                          {},
                                          _hss_connection,
                                          _enum_service,
                                          _acr_factory,
                                          &SNMP::FAKE_INCOMING_SIP_TRANSACTIONS_TABLE,
                                          &SNMP::FAKE_OUTGOING_SIP_TRANSACTIONS_TABLE,
                                          false,
                                          _mmf_service,
                                          _fifc_service,
                                          ifc_configuration,
                                          3000, // Session continue timeout - different from default
                                          6000, // Session terminated timeout - different from default
                                          _sess_term_comm_tracker,
                                          _sess_cont_comm_tracker
                                          );
    _scscf_sproutlet->init();

    _scscf_selector = new SCSCFSelector("sip:scscf.sprout.homedomain", 
                                        string(UT_DIR).append("/test_icscf.json"));
    // Create the I-CSCF Sproutlets.
    _icscf_sproutlet = new ICSCFSproutlet("icscf",
                                          "sip:bgcf@homedomain:5058",
                                          5059,
                                          "sip:icscf.sprout.homedomain:5059;transport=TCP",
                                          _hss_connection,
                                          _acr_factory,
                                          _scscf_selector,
                                          _enum_service,
                                          &SNMP::FAKE_INCOMING_SIP_TRANSACTIONS_TABLE,
                                          &SNMP::FAKE_OUTGOING_SIP_TRANSACTIONS_TABLE,
                                          false
                                          );
    _icscf_sproutlet->init();

    // Create the BGCF Sproutlet.
    _bgcf_sproutlet = new BGCFSproutlet("bgcf",
                                        5054,
                                        "sip:bgcf.homedomain:5054;transport=tcp",
                                        _bgcf_service,
                                        _enum_service,
                                        _acr_factory,
                                        nullptr,
                                        nullptr,
                                        false);

    // Create the MMTEL AppServer.
    _mmtel = new Mmtel("mmtel", _xdm_connection);
    _mmtel_sproutlet = new SproutletAppServerShim(_mmtel,
                                                  5055,
                                                  "sip:mmtel.homedomain:5058;transport=tcp",
                                                  &SNMP::FAKE_INCOMING_SIP_TRANSACTIONS_TABLE,
                                                  &SNMP::FAKE_OUTGOING_SIP_TRANSACTIONS_TABLE,
                                                  "mmtel.homedomain");

    // Create the SproutletProxy.
    std::list<Sproutlet*> sproutlets;
    sproutlets.push_back(_scscf_sproutlet);
    sproutlets.push_back(_icscf_sproutlet);
    sproutlets.push_back(_bgcf_sproutlet);
    sproutlets.push_back(_mmtel_sproutlet);
    std::unordered_set<std::string> additional_home_domains;
    additional_home_domains.insert("sprout.homedomain");
    additional_home_domains.insert("sprout-site2.homedomain");
    additional_home_domains.insert("127.0.0.1");
    _proxy = new SproutletProxy(stack_data.endpt,
                                PJSIP_MOD_PRIORITY_UA_PROXY_LAYER+1,
                                "homedomain",
                                additional_home_domains,
                                sproutlets,
                                std::set<std::string>());
  }

  ~SCSCFTest()
  {
    ::testing::Mock::VerifyAndClearExpectations(_sess_term_comm_tracker);
    ::testing::Mock::VerifyAndClearExpectations(_sess_cont_comm_tracker);

    for (map<string,pjsip_tx_data*>::iterator it = _tdata.begin();
         it != _tdata.end();
         ++it)
    {
      pjsip_tx_data_dec_ref(it->second);
    }

    pjsip_tsx_layer_dump(true);

    // Terminate all transactions
    terminate_all_tsxs(PJSIP_SC_SERVICE_UNAVAILABLE);

    // PJSIP transactions aren't actually destroyed until a zero ms
    // timer fires (presumably to ensure destruction doesn't hold up
    // real work), so poll for that to happen. Otherwise we leak!
    // Allow a good length of time to pass too, in case we have
    // transactions still open. 32s is the default UAS INVITE
    // transaction timeout, so we go higher than that.
    cwtest_advance_time_ms(33000L);
    poll();

    // Stop and restart the layer just in case
    pjsip_tsx_layer_instance()->stop();
    pjsip_tsx_layer_instance()->start();

    // Reset any configuration changes
    URIClassifier::enforce_user_phone = false;
    URIClassifier::enforce_global = false;
    ((SNMP::FakeCounterTable*)_scscf_sproutlet->_routed_by_preloaded_route_tbl)->reset_count();

    delete _hss_connection; _hss_connection = NULL;
    delete _hss_connection_observer; _hss_connection_observer = NULL;
    delete _proxy; _proxy = NULL;
    delete _mmtel_sproutlet; _mmtel_sproutlet = NULL;
    delete _mmtel; _mmtel = NULL;
    delete _bgcf_sproutlet; _bgcf_sproutlet = NULL;
    delete _scscf_sproutlet; _scscf_sproutlet = NULL;
    delete _scscf_selector; _scscf_selector = NULL;
    delete _icscf_sproutlet; _icscf_sproutlet = NULL;
  }

  /// Send a response back through multiple hops in a dialog. The response is
  /// injected at the downstream end of the dialog (the end that the request
  /// flowed towards. The proxied response is received at each hop, the status
  /// code is checked, and it is then re-injected.
  ///
  /// The outbound message queue must be empty when this function is called.
  ///
  /// @param req         - The response to inject at the downstream end.
  /// @param status_code - The status code of the request. This is used to
  ///                      check that the response it passed through at each
  ///                      hop.
  /// @param num_hops    - The number of hops in the dialog.
  void send_response_back_through_dialog(const std::string& response,
                                         int status_code,
                                         int num_hops)
  {
    std::string curr_response = response;

    for (int ii = 0; ii < num_hops; ++ii)
    {
      inject_msg(curr_response);

      ASSERT_EQ(1, txdata_count());
      RespMatcher(status_code).matches(current_txdata()->msg);

      // Render the received message to a string so we can re-inject it. 64kB
      // should be enough space for this.
      char msg_print_buf[0x10000];
      pj_ssize_t len = pjsip_msg_print(current_txdata()->msg,
                                       msg_print_buf,
                                       sizeof(msg_print_buf));
      curr_response.assign(msg_print_buf, len);

      free_txdata();
    }
  }

protected:
  static LocalStore* _local_data_store;
  static FakeChronosConnection* _chronos_connection;
  static SubscriberDataManager* _sdm;
  static AnalyticsLogger* _analytics;
  static FakeHSSConnection* _hss_connection;
  static MockHSSConnection* _hss_connection_observer;
  static FakeXDMConnection* _xdm_connection;
  static BgcfService* _bgcf_service;
  static EnumService* _enum_service;
  static ACRFactory* _acr_factory;
  static MMFService* _mmf_service;
  static FIFCService* _fifc_service;
  static SCSCFSelector* _scscf_selector;
  SCSCFSproutlet* _scscf_sproutlet;
  ICSCFSproutlet* _icscf_sproutlet;
  BGCFSproutlet* _bgcf_sproutlet;
  Mmtel* _mmtel;
  SproutletAppServerShim* _mmtel_sproutlet;
  SproutletProxy* _proxy;
  static MockAsCommunicationTracker* _sess_term_comm_tracker;
  static MockAsCommunicationTracker* _sess_cont_comm_tracker;

  void doTestHeaders(TransportFlow* tpA,
                     bool tpAset,
                     TransportFlow* tpB,
                     bool tpBset,
                     SP::Message& msg,
                     string route,
                     bool expect_100,
                     bool expect_trusted_headers_on_requests,
                     bool expect_trusted_headers_on_responses,
                     bool expect_orig,
                     bool pcpi);
  void doAsOriginated(SP::Message& msg, bool expect_orig);
  void doAsOriginated(const std::string& msg, bool expect_orig);
  void doFourAppServerFlow(std::string record_route_regex, bool app_servers_record_route=false);
  void doSuccessfulFlow(SP::Message& msg,
                        testing::Matcher<string> uri_matcher,
                        list<HeaderMatcher> headers,
                        bool include_ack_and_bye=true,
                        list<HeaderMatcher> rsp_hdrs = list<HeaderMatcher>());
  void doFastFailureFlow(SP::Message& msg, int st_code);
  void doSlowFailureFlow(SP::Message& msg, int st_code, std::string body = "", std::string reason = "");
  void setupForkedFlow(SP::Message& msg);
  list<string> doProxyCalculateTargets(int max_targets);
};

LocalStore* SCSCFTest::_local_data_store;
FakeChronosConnection* SCSCFTest::_chronos_connection;
SubscriberDataManager* SCSCFTest::_sdm;
AnalyticsLogger* SCSCFTest::_analytics;
FakeHSSConnection* SCSCFTest::_hss_connection;
MockHSSConnection* SCSCFTest::_hss_connection_observer;
FakeXDMConnection* SCSCFTest::_xdm_connection;
BgcfService* SCSCFTest::_bgcf_service;
EnumService* SCSCFTest::_enum_service;
ACRFactory* SCSCFTest::_acr_factory;
MMFService* SCSCFTest::_mmf_service;
FIFCService* SCSCFTest::_fifc_service;
SCSCFSelector* SCSCFTest::_scscf_selector;
MockAsCommunicationTracker* SCSCFTest::_sess_term_comm_tracker;
MockAsCommunicationTracker* SCSCFTest::_sess_cont_comm_tracker;

using SP::Message;

void SCSCFTest::doFourAppServerFlow(std::string record_route_regex, bool app_servers_record_route)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:4.2.3.4:56788;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>QWERTY_UIOP</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:sholes.example.com</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>3</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:6.2.3.4:56786;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(StrEq("sip:4.2.3.4:56788;transport=UDP")));
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(StrEq("sip:1.2.3.4:56789;transport=UDP")));
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(StrEq("sip:5.2.3.4:56787;transport=UDP")));
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(StrEq("sip:6.2.3.4:56786;transport=UDP")));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "4.2.3.4", 56788);
  TransportFlow tpAS3(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);
  TransportFlow tpAS4(TransportFlow::Protocol::UDP, stack_data.scscf_port, "6.2.3.4", 56786);

  pjsip_rr_hdr* as1_rr_hdr = pjsip_rr_hdr_create(stack_data.pool);
  as1_rr_hdr->name_addr.uri = (pjsip_uri*)pjsip_sip_uri_create(stack_data.pool, false);
  ((pjsip_sip_uri*)as1_rr_hdr->name_addr.uri)->host = pj_str("1.2.3.4");

  pjsip_rr_hdr* as2_rr_hdr = pjsip_rr_hdr_create(stack_data.pool);
  as2_rr_hdr->name_addr.uri = (pjsip_uri*)pjsip_sip_uri_create(stack_data.pool, false);
  ((pjsip_sip_uri*)as2_rr_hdr->name_addr.uri)->host = pj_str("4.2.3.4");

  pjsip_rr_hdr* as3_rr_hdr = pjsip_rr_hdr_create(stack_data.pool);
  as3_rr_hdr->name_addr.uri = (pjsip_uri*)pjsip_sip_uri_create(stack_data.pool, false);
  ((pjsip_sip_uri*)as3_rr_hdr->name_addr.uri)->host = pj_str("5.2.3.4");

  pjsip_rr_hdr* as4_rr_hdr = pjsip_rr_hdr_create(stack_data.pool);
  as4_rr_hdr->name_addr.uri = (pjsip_uri*)pjsip_sip_uri_create(stack_data.pool, false);
  ((pjsip_sip_uri*)as4_rr_hdr->name_addr.uri)->host = pj_str("6.2.3.4");

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");

  if (app_servers_record_route)
  {
    pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)as1_rr_hdr);
  }

  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS2
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS2.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:4\\.2\\.3\\.4:56788;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));

  // ---------- AS2 sends a 100 Trying to indicate it has received the request.
  string fresp2 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp2, &tpAS2);

  // ---------- AS2 turns it around (acting as proxy)
  if (app_servers_record_route)
  {
    pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)as2_rr_hdr);
  }

  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS2);
  free_txdata();

  // 100 Trying goes back to AS2
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS2.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS3 - From this point on, we're in terminating mode.
  SCOPED_TRACE("INVITE (3)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS3.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS3 sends a 100 Trying to indicate it has received the request.
  string fresp3 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp3, &tpAS3);

  // ---------- AS3 turns it around (acting as proxy)
  if (app_servers_record_route)
  {
    pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)as3_rr_hdr);
  }

  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS3);
  free_txdata();

  // 100 Trying goes back to AS3
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS3.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS4
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS4.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:6\\.2\\.3\\.4:56786;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS4 sends a 100 Trying to indicate it has received the request.
  string fresp4 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp4, &tpAS4);

  // ---------- AS4 turns it around (acting as proxy)
  if (app_servers_record_route)
  {
    pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)as4_rr_hdr);
  }

  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS4);
  free_txdata();

  // 100 Trying goes back to AS4
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS4.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (Z)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // ---------- Bono sends a 100 Trying to indicate it has received the request.
  string fresp_bono = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp_bono, &tpBono);

  EXPECT_THAT(get_headers(out, "Record-Route"), testing::MatchesRegex(record_route_regex));

  // Send a 200 OK back down the line to finish the transaction. This is so that
  // AS communication tracking works correctly. There are a total of 5 hops in
  // total.
  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 5);

  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}

// Test flows into Sprout (S-CSCF), in particular for header stripping.
// Check the transport each message is on, and the headers.
// Test a call from Alice to Bob.
void SCSCFTest::doTestHeaders(TransportFlow* tpA,  //< Alice's transport.
                              bool tpAset,         //< Expect all requests to Alice on same transport?
                              TransportFlow* tpB,  //< Bob's transport.
                              bool tpBset,         //< Expect all requests to Bob on same transport?
                              SP::Message& msg,    //< Message to use for testing.
                              string route,        //< Route header to be used on INVITE
                              bool expect_100,     //< Will we get a 100 Trying?
                              bool expect_trusted_headers_on_requests, //< Should P-A-N-I/P-V-N-I be passed on requests?
                              bool expect_trusted_headers_on_responses, //< Should P-A-N-I/P-V-N-I be passed on responses?
                              bool expect_orig,    //< Should we expect the INVITE to be marked originating?
                              bool pcpi)           //< Should we expect a P-Called-Party-ID?
{
  SCOPED_TRACE("doTestHeaders");
  pjsip_msg* out;
  pjsip_tx_data* invite = NULL;
  pjsip_tx_data* prack = NULL;

  // Extra fields to insert in all requests and responses.
  string pani = "P-Access-Network-Info: ietf-carrier-pigeon;rfc=1149";
  string pvni = "P-Visited-Network-Id: other.net, \"Other Network\"";
  string pvani = pani + "\r\n" + pvni;

  if (!msg._extra.empty())
  {
    msg._extra.append("\r\n");
  }

  msg._extra.append(pani);
  msg._extra.append("\r\n");
  msg._extra.append(pvni);

  // ---------- Send INVITE C->X
  SCOPED_TRACE("INVITE");
  msg._method = "INVITE";

  if (route != "")
  {
    msg._route = route;
  }

  inject_msg(msg.get_request(), tpA);
  poll();
  ASSERT_EQ(expect_100 ? 2 : 1, txdata_count());

  if (expect_100)
  {
    // 100 Trying goes back C<-X
    out = current_txdata()->msg;
    RespMatcher(100).matches(out);
    tpA->expect_target(current_txdata(), true);  // Requests always come back on same transport
    msg.set_route(out);

    // Don't bother testing P-Access-Network-Info or P-Visited-Network-Id,
    // because they never get inserted into such messages.
    free_txdata();
  }

  // INVITE passed on X->S
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("INVITE").matches(out));
  tpB->expect_target(current_txdata(), tpBset);

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_requests ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "INVITE";
  EXPECT_EQ(expect_trusted_headers_on_requests ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "INVITE";

  // Check originating.
  if (expect_orig)
  {
    EXPECT_THAT(get_headers(out, "Route"), HasSubstr(";orig"));
  }
  else
  {
    EXPECT_THAT(get_headers(out, "Route"), Not(HasSubstr(";orig")));
  }

  // Check P-Called-Party-ID
  EXPECT_EQ(pcpi ? "P-Called-Party-ID: <" + msg._toscheme + ":" + msg._to + "@" + msg._todomain + ">" : "", get_headers(out, "P-Called-Party-ID"));

  invite = pop_txdata();

  // ---------- Send 183 Session Progress back X<-S
  SCOPED_TRACE("183 Session Progress");
  inject_msg(respond_to_txdata(invite, 183, "", pvani), tpB);
  ASSERT_EQ(1, txdata_count());

  // 183 goes back C<-X
  out = current_txdata()->msg;
  RespMatcher(183).matches(out);
  tpA->expect_target(current_txdata(), true);
  msg.set_route(out);
  msg._cseq++;

  // Check P-Access-Network-Info and P-Visited-Network-Id
  EXPECT_EQ(expect_trusted_headers_on_responses ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "183 Session Progress";
  EXPECT_EQ(expect_trusted_headers_on_responses ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "183 Session Progress";

  free_txdata();

  // Send PRACK C->X
  SCOPED_TRACE("PRACK");
  msg._method = "PRACK";
  inject_msg(msg.get_request(), tpA);
  poll();
  ASSERT_EQ(1, txdata_count());

  // PRACK passed on X->S
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("PRACK").matches(out));
  tpB->expect_target(current_txdata(), tpBset);

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_requests ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "PRACK";
  EXPECT_EQ(expect_trusted_headers_on_requests ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "PRACK";

  prack = pop_txdata();

  // ---------- Send 200 OK back X<-S
  SCOPED_TRACE("200 OK (PRACK)");
  inject_msg(respond_to_txdata(prack, 200, "", pvani), tpB);
  ASSERT_EQ(1, txdata_count());

  // OK goes back C<-X
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpA->expect_target(current_txdata(), true);
  msg.set_route(out);
  msg._cseq++;

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_responses ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "200 OK (PRACK)";
  EXPECT_EQ(expect_trusted_headers_on_responses ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "200 OK (PRACK)";

  free_txdata();

  // ---------- Send 200 OK back X<-S
  SCOPED_TRACE("200 OK (INVITE)");
  inject_msg(respond_to_txdata(invite, 200, "", pvani), tpB);
  ASSERT_EQ(1, txdata_count());

  // OK goes back C<-X
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpA->expect_target(current_txdata(), true);
  msg.set_route(out);
  msg._cseq++;

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_responses ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "200 OK (INVITE)";
  EXPECT_EQ(expect_trusted_headers_on_responses ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "200 OK (INVITE)";

  free_txdata();

  // ---------- Send ACK C->X
  SCOPED_TRACE("ACK");
  msg._method = "ACK";
  inject_msg(msg.get_request(), tpA);
  poll();
  ASSERT_EQ(1, txdata_count());

  // ACK passed on X->S
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  tpB->expect_target(current_txdata(), tpBset);

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_requests ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "ACK";
  EXPECT_EQ(expect_trusted_headers_on_requests ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "ACK";

  free_txdata();

  // ---------- Send a retransmission of that 200 OK back X<-S.  Should be processed statelessly.
  SCOPED_TRACE("200 OK (INVITE) (rexmt)");
  inject_msg(respond_to_txdata(invite, 200, "", pvani), tpB);
  pjsip_tx_data_dec_ref(invite);
  invite = NULL;
  ASSERT_EQ(1, txdata_count());

  // OK goes back C<-X
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpA->expect_target(current_txdata(), true);
  msg.set_route(out);
  msg._cseq++;

  // Check P-Access-Network-Info and P-Visited-Network-Id. These will always be stripped,
  // because we handle these retransmissions statelessly and hence don't have any info on
  // trust boundary handling.
  //EXPECT_EQ("", get_headers(out, "P-Access-Network-Info")) << "200 OK (INVITE) (rexmt)";
  //EXPECT_EQ("", get_headers(out, "P-Visited-Network-Id")) << "200 OK (INVITE) (rexmt)";

  free_txdata();

  // ---------- Send a reINVITE in the reverse direction. X<-S

  // ---------- Send a subsequent request. C->X
  SCOPED_TRACE("BYE");
  msg._method = "BYE";
  inject_msg(msg.get_request(), tpA);
  poll();

  // BYE passed on X->S
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("BYE").matches(out));
  tpB->expect_target(current_txdata(), tpBset);

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_requests ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "BYE";
  EXPECT_EQ(expect_trusted_headers_on_requests ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "BYE";

  // ---------- Send a reply to that X<-S
  SCOPED_TRACE("200 OK (BYE)");
  inject_msg(respond_to_current_txdata(200, "", pvani), tpB);
  poll();
  ASSERT_EQ(1, txdata_count());

  // Reply passed on C<-X
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpA->expect_target(current_txdata(), true);

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_responses ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "200 OK (BYE)";
  EXPECT_EQ(expect_trusted_headers_on_responses ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "200 OK (BYE)";

  free_txdata();

  // ---------- Send INVITE C->X (this is an attempt to establish a second dialog)
  SCOPED_TRACE("INVITE (#2)");
  msg._method = "INVITE";

  if (route != "")
  {
    msg._route = route;
  }
  msg._unique++;
  inject_msg(msg.get_request(), tpA);
  poll();
  ASSERT_EQ(expect_100 ? 2 : 1, txdata_count());

  if (expect_100)
  {
    // 100 Trying goes back C<-X
    out = current_txdata()->msg;
    RespMatcher(100).matches(out);
    tpA->expect_target(current_txdata(), true);

    // Don't bother testing P-Access-Network-Info or P-Visited-Network-Id, because this is point-to-point.
    free_txdata();
  }

  // INVITE passed on X->S
  SCOPED_TRACE("INVITE (S#2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("INVITE").matches(out));
  tpB->expect_target(current_txdata(), tpBset);

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_requests ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "INVITE (#2)";
  EXPECT_EQ(expect_trusted_headers_on_requests ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "INVITE (#2)";

  invite = pop_txdata();

  // ---------- Send 404 Not Found back X<-S
  SCOPED_TRACE("404 Not Found (INVITE #2)");
  inject_msg(respond_to_txdata(invite, 404, "", pvani), tpB);
  poll();
  ASSERT_EQ(2, txdata_count());

  // ACK autogenerated X->S
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  tpB->expect_target(current_txdata(), tpBset);

  // Don't check P-Access-Network-Info or P-Visited-Network-Id, because it's point-to-point.

  free_txdata();

  // 404 goes back C<-X
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  tpA->expect_target(current_txdata(), true);
  msg.set_route(out);
  msg._cseq++;

  // Check P-Access-Network-Info and P-Visited-Network-Id.
  EXPECT_EQ(expect_trusted_headers_on_responses ? pani : "",
            get_headers(out, "P-Access-Network-Info")) << "404 Not Found (INVITE #2)";
  EXPECT_EQ(expect_trusted_headers_on_responses ? pvni : "",
            get_headers(out, "P-Visited-Network-Id")) << "404 Not Found (INVITE #2)";

  free_txdata();

  // ---------- Send ACK C->X
  SCOPED_TRACE("ACK (#2)");
  msg._method = "ACK";
  inject_msg(msg.get_request(), tpA);
  poll();
  ASSERT_EQ(0, txdata_count());
  // should be swallowed by core.
}


/// Test a message results in a successful flow. The outgoing INVITE's
/// URI is verified.
void SCSCFTest::doSuccessfulFlow(Message& msg,
                                 testing::Matcher<string> uri_matcher,
                                 list<HeaderMatcher> headers,
                                 bool include_ack_and_bye,
                                 list<HeaderMatcher> rsp_headers)
{
  SCOPED_TRACE("");
  pjsip_msg* out;

  // Send INVITE
  inject_msg(msg.get_request());
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // INVITE passed on
  out = current_txdata()->msg;
  ReqMatcher req("INVITE");
  ASSERT_NO_FATAL_FAILURE(req.matches(out));

  // Do checks on what gets passed through:
  EXPECT_THAT(req.uri(), uri_matcher);
  for (list<HeaderMatcher>::iterator iter = headers.begin(); iter != headers.end(); ++iter)
  {
    iter->match(out);
  }

  // Send 200 OK back
  inject_msg(respond_to_current_txdata(200));
  ASSERT_EQ(1, txdata_count());

  // OK goes back
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  for (list<HeaderMatcher>::iterator iter = rsp_headers.begin();
       iter != rsp_headers.end();
       ++iter)
  {
    iter->match(out);
  }

  msg.set_route(out);
  msg._cseq++;
  free_txdata();

  // If we're testing Sprout functionality, we want to exclude the ACK
  // and BYE requests, as Sprout wouldn't see them in normal circumstances.
  if (include_ack_and_bye)
  {
    // Send ACK
    msg._method = "ACK";
    inject_msg(msg.get_request());
    poll();
    ASSERT_EQ(1, txdata_count());
    out = current_txdata()->msg;
    ReqMatcher req2("ACK");
    ASSERT_NO_FATAL_FAILURE(req2.matches(out));
    free_txdata();

    // Send a subsequent request.
    msg._method = "BYE";
    inject_msg(msg.get_request());
    poll();
    ASSERT_EQ(1, txdata_count());
    out = current_txdata()->msg;
    ReqMatcher req3("BYE");
    ASSERT_NO_FATAL_FAILURE(req3.matches(out));

    // Send a reply to that.
    inject_msg(respond_to_current_txdata(200));
    poll();
    ASSERT_EQ(1, txdata_count());
    out = current_txdata()->msg;
    RespMatcher(200).matches(out);

    free_txdata();
  }
}

/// Test a message results in an immediate failure.
void SCSCFTest::doFastFailureFlow(Message& msg, int st_code)
{
  SCOPED_TRACE("");

  // Send INVITE
  inject_msg(msg.get_request());
  ASSERT_EQ(1, txdata_count());
  pjsip_msg* out;

  // error goes back
  out = current_txdata()->msg;
  RespMatcher(st_code).matches(out);
  free_txdata();
}

/// Test a message results in a 100 then a failure.
void SCSCFTest::doSlowFailureFlow(Message& msg,
                                  int st_code,
                                  std::string body,
                                  std::string reason)
{
  SCOPED_TRACE("");

  // Send INVITE
  inject_msg(msg.get_request());
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // error goes back
  out = current_txdata()->msg;
  RespMatcher(st_code, body, reason).matches(out);
  free_txdata();
}

TEST_F(SCSCFTest, TestSimpleMainline)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  Message msg;
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);

  // This is a terminating call so should not result in a session setup time
  // getting tracked.
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);

  // It also shouldn't result in any forked INVITEs
  EXPECT_EQ(0, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_forked_invite_tbl)->_count);
}

TEST_F(SCSCFTest, TestSimpleMainlineRemoteSite)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  Message msg;
  msg._route = "Route: <sip:scscf.sprout-site2.homedomain;transport=tcp;lr>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Record-Route", "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-term>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);

  // Make sure that the HTTP request sent to homestead contains the correct S-CSCF URI.
  EXPECT_TRUE(_hss_connection->url_was_requested("/impu/sip%3A6505551234%40homedomain/reg-data", "{\"reqtype\": \"call\", \"server_name\": \"sip:scscf.sprout-site2.homedomain:5058;transport=TCP\"}"));
}

// Send a request where the URI is for the same port as a Sproutlet,
// but a different host. We should deal with this sensibly (as opposed
// to e.g. looping forever until we crash).
TEST_F(SCSCFTest, ReqURIMatchesSproutletPort)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  Message msg;
  msg._requri = "sip:254.253.252.251:5058";
  msg._route = "Route: <sip:sprout.homedomain;transport=tcp;lr;billing-role=charge-term>";
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex("sip:254.253.252.251:5058"), hdrs, false);
}

// Test flows into Sprout (S-CSCF), in particular for header stripping.
TEST_F(SCSCFTest, TestMainlineHeadersSprout)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // INVITE from anywhere to anywhere.
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345";
  doTestHeaders(_tp_default, false, _tp_default, false, msg, "", true, true, true, false, true);
}

TEST_F(SCSCFTest, TestNotRegisteredTo)
{
  SCOPED_TRACE("");
  Message msg;
  doSlowFailureFlow(msg, 404);
}

TEST_F(SCSCFTest, TestBadScheme)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  Message msg;
  msg._toscheme = "sips";
  doFastFailureFlow(msg, 416);  // bad scheme
}

TEST_F(SCSCFTest, TestBarredCaller)
{
  // Tests that a call attempt from a barred caller is rejected with a 403.
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "REGISTERED",
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "  <PublicIdentity>"
                                   "    <Identity>sip:6505551000@homedomain</Identity>"
                                   "    <BarringIndication>1</BarringIndication>"
                                   "  </PublicIdentity>\n"
                                   "  <InitialFilterCriteria>\n"
                                   "    <Priority>1</Priority>\n"
                                   "    <TriggerPoint>\n"
                                   "      <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                   "      <SPT>\n"
                                   "        <ConditionNegated>1</ConditionNegated>\n"
                                   "        <Group>0</Group>\n"
                                   "        <Method>INVITE</Method>\n"
                                   "        <Extension></Extension>\n"
                                   "      </SPT>\n"
                                   "    </TriggerPoint>\n"
                                   "    <ApplicationServer>\n"
                                   "      <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                   "      <DefaultHandling>0</DefaultHandling>\n"
                                   "    </ApplicationServer>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  doSlowFailureFlow(msg, 403);
}

TEST_F(SCSCFTest, TestBarredCallee)
{
  // Tests that a call to a barred callee is rejected with a 404.
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity><BarringIndication>1</BarringIndication></PublicIdentity>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>1</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  Message msg;
  doSlowFailureFlow(msg, 404);
}

// Test that a call from an IMPU that belongs to a barred wildcarded public
// identity is rejected with a 403 (forbidden). The IMPU isn't included as
// a non-distinct IMPU in the HSS response.
TEST_F(SCSCFTest, TestBarredWildcardCaller)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551000@homedomain",
                                   "call",
                                   "REGISTERED",
                                   IMS_SUB_BARRED_WILDCARD);
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  doSlowFailureFlow(msg, 403);
}

// Test that a call to an IMPU that belongs to a barred wildcarded public
// identity is rejected with a 404 (not found). The IMPU isn't included as
// a non-distinct IMPU in the HSS response.
TEST_F(SCSCFTest, TestBarredWildcardCallee)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551000@homedomain",
                                   "call",
                                   "REGISTERED",
                                   IMS_SUB_BARRED_WILDCARD);
  Message msg;
  doSlowFailureFlow(msg, 404);
}

// Test that a call from a barred IMPU that belongs to a non-barred wildcarded
// public identity is rejected with a 403 (forbidden). The IMPU is included as
// a non-distinct IMPU in the HSS response.
TEST_F(SCSCFTest, TestWildcardBarredCaller)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551000@homedomain",
                                   "call",
                                   "REGISTERED",
                                   IMS_SUB_BARRED_IMPU_IN_WILDCARD);
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  doSlowFailureFlow(msg, 403);
}

// Test that a call to a barred IMPU that belongs to a non-barred wildcarded
// public identity is rejected with a 404. The IMPU is included as a
// non-distinct IMPU in the HSS response.
TEST_F(SCSCFTest, TestWildcardBarredCallee)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551000@homedomain",
                                   "call",
                                   "REGISTERED",
                                   IMS_SUB_BARRED_IMPU_IN_WILDCARD);
  Message msg;
  doSlowFailureFlow(msg, 404);
}

// Test that a call from a barred IMPU that belongs to a non-barred wildcarded
// public identity is rejected with a 403. The HSS response includes multiple
// wildcard identities that could match the IMPU, so this checks that the
// correct identity is selected.
TEST_F(SCSCFTest, TestBarredMultipleWildcardCaller)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551000@homedomain",
                                   "call",
                                   "REGISTERED",
                                   IMS_SUB_BARRED_MULTIPLE_WILDCARD);
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  doSlowFailureFlow(msg, 403);
}

// Test that a call to a barred IMPU that belongs to a non-barred wildcarded
// public identity is rejected with a 404. The HSS response includes multiple
// wildcard identities that could match the IMPU, so this checks that the
// correct identity is selected.
TEST_F(SCSCFTest, TestBarredMultipleWildcardCallee)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:6505551000@homedomain",
                                   "call",
                                   "REGISTERED",
                                   IMS_SUB_BARRED_MULTIPLE_WILDCARD);
  Message msg;
  doSlowFailureFlow(msg, 404);
}

TEST_F(SCSCFTest, TestSimpleTelURI)
{
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  Message msg;
  msg._toscheme = "tel";
  msg._to = "16505551234";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*16505551234@ut.cw-ngv.com.*"), hdrs, false);

  // Successful originating call.  We should have tracked a single session
  // setup time.
  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}

// Test that a successful originating video call results in the correct stats
// being tracked.
TEST_F(SCSCFTest, TestSimpleTelURIVideo)
{
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  Message msg;
  msg._toscheme = "tel";
  msg._to = "16505551234";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._body = "\r\nv=0\r\no=Andrew 2890844526 2890844526 IN IP4 10.120.42.3\r\nc=IN IP4 10.120.42.3\r\nt=0 0\r\nm=audio 49170 RTP/AVP 0 8 97\r\na=rtpmap:0 PCMU/8000\r\nm=video 51372 RTP/AVP 31 32\r\na=rtpmap:31 H261/90000\r\n";
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*16505551234@ut.cw-ngv.com.*"), hdrs, false);

  // Successful originating call.  We should have tracked a single session
  // setup time.
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}


TEST_F(SCSCFTest, TestTerminatingTelURI)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("tel:6505551235", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "<PublicIdentity><Identity>tel:6505551235</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>1</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // Send a terminating INVITE for a subscriber with a tel: URI
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._todomain = "";
  msg._requri = "tel:6505551235";

  msg._method = "INVITE";
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob"), hdrs, false);
}

TEST_F(SCSCFTest, TestTelURIWildcard)
{
  _hss_connection->set_impu_result("tel:6505551235", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>tel:6505552345</Identity></PublicIdentity>"
                                "<PublicIdentity><Identity>tel:65055522!.*!</Identity></PublicIdentity>"
                                "<PublicIdentity><Identity>tel:65055512!.*!</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // Send a terminating INVITE for a subscriber with a tel: URI
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._todomain = "";
  msg._requri = "tel:6505551235";

  msg._method = "INVITE";
  list<HeaderMatcher> hdrs;

  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();
  ASSERT_EQ(1, txdata_count());

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  pjsip_tx_data* tdata = current_txdata();
  out = tdata->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(tdata, false);
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  string fresp1 = respond_to_txdata(tdata, 404);
  inject_msg(fresp1, &tpAS1);
  ASSERT_EQ(3, txdata_count());
  free_txdata();
  free_txdata();
  ASSERT_EQ(1, txdata_count());

  // 100 Trying goes back to bono
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  free_txdata();
  ASSERT_EQ(0, txdata_count());
}

TEST_F(SCSCFTest, TestMultipleServiceProfiles)
{
  _hss_connection->set_impu_result("tel:6505551235", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>tel:6505552345</Identity></PublicIdentity>"
                                "<PublicIdentity><Identity>tel:65055512!.*!</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:5.6.7.8:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile><ServiceProfile>\n"
                                "<PublicIdentity><Identity>tel:6505551235</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // Send a terminating INVITE for a subscriber with a tel: URI
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._todomain = "";
  msg._requri = "tel:6505551235";

  msg._method = "INVITE";
  list<HeaderMatcher> hdrs;

  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();
  ASSERT_EQ(1, txdata_count());

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  pjsip_tx_data* tdata = current_txdata();
  out = tdata->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(tdata, false);
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  string fresp1 = respond_to_txdata(tdata, 404);
  inject_msg(fresp1, &tpAS1);
  ASSERT_EQ(3, txdata_count());
  free_txdata();
  free_txdata();
  ASSERT_EQ(1, txdata_count());

  // 100 Trying goes back to bono
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  free_txdata();
  ASSERT_EQ(0, txdata_count());
}

TEST_F(SCSCFTest, TestMultipleAmbiguousServiceProfiles)
{
  _hss_connection->set_impu_result("tel:6505551235", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>tel:6505552345</Identity></PublicIdentity>"
                                "<PublicIdentity><Identity>tel:65055512!.*!</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile><ServiceProfile>\n"
                                "<PublicIdentity><Identity>tel:650555123!.*!</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:5.6.7.8:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // Send a terminating INVITE for a subscriber with a tel: URI
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._todomain = "";
  msg._requri = "tel:6505551235";

  msg._method = "INVITE";
  list<HeaderMatcher> hdrs;

  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();
  ASSERT_EQ(1, txdata_count());

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  pjsip_tx_data* tdata = current_txdata();
  out = tdata->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(tdata, false);
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  string fresp1 = respond_to_txdata(tdata, 404);
  inject_msg(fresp1, &tpAS1);
  ASSERT_EQ(3, txdata_count());
  free_txdata();
  free_txdata();
  ASSERT_EQ(1, txdata_count());

  // 100 Trying goes back to bono
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  free_txdata();
  ASSERT_EQ(0, txdata_count());
}

TEST_F(SCSCFTest, TestNoMoreForwards)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  Message msg;
  msg._forwards = 1;
  doFastFailureFlow(msg, 483); // too many hops
}

TEST_F(SCSCFTest, TestNoMoreForwards2)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  Message msg;
  msg._forwards = 0;
  doFastFailureFlow(msg, 483); // too many hops
}

TEST_F(SCSCFTest, TestTransportShutdown)
{
  SCOPED_TRACE("");
  pjsip_tx_data* tdata;

  // Create a TCP connection to the listening port.
  TransportFlow* tp = new TransportFlow(TransportFlow::Protocol::TCP,
                                        stack_data.scscf_port,
                                        "1.2.3.4",
                                        49152);

  // Inject an INVITE request on a transport which is shutting down.  It is safe
  // to call pjsip_transport_shutdown on a TCP transport as the TransportFlow
  // keeps a reference to the transport so it won't actually be destroyed until
  // the TransportFlow is destroyed.
  pjsip_transport_shutdown(tp->transport());

  Message msg;
  msg._method = "INVITE";
  msg._requri = "sip:bob@awaydomain";
  msg._from = "alice";
  msg._to = "bob";
  msg._todomain = "awaydomain";
  msg._via = tp->to_string(false);
  msg._route = "Route: <sip:proxy1.awaydomain;transport=TCP;lr>";
  inject_msg(msg.get_request(), tp);

  // Check the 504 Service Unavailable response.
  ASSERT_EQ(1, txdata_count());
  tdata = current_txdata();
  RespMatcher(503).matches(tdata->msg);
  tp->expect_target(tdata);
  free_txdata();

  // Send an ACK to complete the UAS transaction.
  msg._method = "ACK";
  inject_msg(msg.get_request(), tp);

  delete tp;
}

TEST_F(SCSCFTest, TestStrictRouteThrough)
{
  SCOPED_TRACE("");
  // This message is passing through this proxy; it's not local
  Message msg;
  add_host_mapping("intermediate.com", "10.10.10.1");
  add_host_mapping("destination.com", "10.10.10.2");
  msg._route = "";
  msg._extra = "Route: <sip:nexthop@intermediate.com;transport=tcp>\r\nRoute: <sip:lasthop@destination.com>";
  msg._to = "lasthop";
  msg._todomain = "destination.com";
  msg._requri = "sip:6505551234@nonlocaldomain";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route", ".*lasthop@destination.com.*", ".*6505551234@nonlocaldomain.*"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*nexthop@intermediate.com.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestNonLocal)
{
  SCOPED_TRACE("");
  // This message is passing through this proxy; it's not local
  add_host_mapping("destination.com", "10.10.10.2");
  Message msg;
  msg._route = "";
  msg._to = "lasthop";
  msg._todomain = "destination.com";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*lasthop@destination\\.com.*"), hdrs);

  // Add another test where the nonlocal domain doesn't contain a period. This
  // is for code coverage.
  add_host_mapping("destination", "10.10.10.3");
  Message msg2;
  msg2._route = "";
  msg2._to = "lasthop";
  msg2._todomain = "destination";
  list<HeaderMatcher> hdrs2;
  hdrs2.push_back(HeaderMatcher("Route"));
  doSuccessfulFlow(msg2, testing::MatchesRegex(".*lasthop@destination.*"), hdrs2);
}

TEST_F(SCSCFTest, TestTerminatingPCV)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Test that a segfault previously seen when not doing originating
  // handling on a call with a P-Charging-Vector does not reoccur.
  Message msg;
  msg._extra = "P-Charging-Vector: icid-value=3";
  msg._to = "lasthop";
  msg._todomain = "destination.com";
  msg._requri = "sip:6505551234@homedomain";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*"), hdrs);
}

TEST_F(SCSCFTest, DISABLED_TestLooseRoute)  // @@@KSW not quite - how does this work again?
{
  SCOPED_TRACE("");
  Message msg;
  msg._extra = "Route: <sip:nexthop@anotherdomain;lr>\r\nRoute: <sip:lasthop@destination.com;lr>";
  msg._to = "lasthop";
  msg._todomain = "destination.com";
  msg._requri = "sip:6505551234@homedomain";
  list<HeaderMatcher> hdrs;
//  hdrs.push_back(HeaderMatcher("Route", ".*lasthop@destination.*"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*lasthop@destination.com.*"), hdrs);
}

TEST_F(SCSCFTest, TestExternal)
{
  SCOPED_TRACE("");
  Message msg;
  msg._to = "+15108580271";
  msg._todomain = "ut.cw-ngv.com";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580271@ut.cw-ngv.com.*"), hdrs);
}

// Test is disabled because there is no Route header, so request is treated as
// terminating request, but domain in RequestURI is not local, so we don't
// provide any services to the user, so therefore shouldn't add a Record-Route.
TEST_F(SCSCFTest, DISABLED_TestExternalRecordRoute)
{
  SCOPED_TRACE("");
  Message msg;
  msg._to = "+15108580271";
  msg._todomain = "ut.cw-ngv.com";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Record-Route", "Record-Route: <sip:sprout.homedomain:5058;transport=TCP;lr;charge-term>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*"), hdrs);
}

TEST_F(SCSCFTest, TestEnumExternalSuccess)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  Message msg;
  msg._to = "+15108580271";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  // Skip the ACK and BYE on this request by setting the last
  // parameter to false, as we're only testing Sprout functionality
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580271@ut.cw-ngv.com.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestNoEnumWhenGRUU)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A%2B15108580271%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  register_uri(_sdm, _hss_connection, "+15108580271", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", 30, "abcd");

  Message msg;
  msg._to = "+15108580271";
  msg._todomain += ";gr=abcd";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;

  // Even though "+15108580271" is configured for ENUM, the presence
  // of a GRUU parameter should indicate to Sprout that this wasn't
  // a string of dialled digits - so we won't do an ENUM lookup and
  // will route to the local subscriber.
  doSuccessfulFlow(msg, testing::MatchesRegex("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob"), hdrs, false);
}

TEST_F(SCSCFTest, TestGRUUFailure)
{
  // Identical to TestNoEnumWhenGRUU, except that the registered
  // binding in this test has a different instance-id ("abcde" nor
  // "abcd"), so the GRUU doesn't match and the call should fail with
  // a 480 error.
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A%2B15108580271%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  register_uri(_sdm, _hss_connection, "+15108580271", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", 30, "abcde");

  Message msg;
  msg._to = "+15108580271";
  msg._todomain += ";gr=abcd";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");

  doSlowFailureFlow(msg, 480);
}

// Various ENUM tests - these use the test_stateful_proxy_enum.json file
// TODO - these want tidying up (maybe make the enum service a mock? at least make it so
// there are separate number ranges used in each test).
TEST_F(SCSCFTest, TestEnumExternalSuccessFromFromHeader)
{
  SCOPED_TRACE("");
  Message msg;
  _hss_connection->set_impu_result("sip:+15108581234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  msg._to = "+15108580271";
  msg._from = "+15108581234";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>";

  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  // Skip the ACK and BYE on this request by setting the last
  // parameter to false, as we're only testing Sprout functionality
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580271@ut.cw-ngv.com.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestEnumExternalOffNetDialingAllowed)
{
  SCOPED_TRACE("");
  Message msg;
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  msg._to = "+15108580271";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  // Skip the ACK and BYE on this request by setting the last
  // parameter to false, as we're only testing Sprout functionality
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580271@ut.cw-ngv.com.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestEnumUserPhone)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  URIClassifier::enforce_user_phone = true;
  Message msg;
  msg._to = "+15108580271";
  msg._requri = "sip:+15108580271@homedomain;user=phone";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  // Skip the ACK and BYE on this request by setting the last
  // parameter to false, as we're only testing Sprout functionality
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580271@ut.cw-ngv.com.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestEnumNoUserPhone)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  URIClassifier::enforce_user_phone = true;
  Message msg;
  msg._to = "+15108580271";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSlowFailureFlow(msg, 404);
}

TEST_F(SCSCFTest, TestEnumLocalNumber)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  URIClassifier::enforce_global = true;
  Message msg;
  msg._to = "15108580271";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSlowFailureFlow(msg, 404);
}

TEST_F(SCSCFTest, TestEnumLocalTelURI)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  URIClassifier::enforce_global = true;
  Message msg;
  msg._to = "16505551234;npdi";
  msg._toscheme = "tel";
  msg._todomain = "";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  // ENUM fails and wr route to the BGCF, but there are no routes so the call
  // is rejected.
  doSlowFailureFlow(msg, 404, "", "No route to target");
}

TEST_F(SCSCFTest, TestEnumLocalSIPURINumber)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  URIClassifier::enforce_global = true;
  Message msg;
  msg._to = "15108580271;npdi";
  msg._requri = "sip:15108580271;npdi@homedomain;user=phone";
  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  // ENUM fails and wr route to the BGCF, but there are no routes so the call
  // is rejected.
  doSlowFailureFlow(msg, 404, "", "No route to target");
}

// Test where the the ENUM lookup returns NP data. The request URI
// is changed, and the request is routed to the BGCF.
TEST_F(SCSCFTest, TestEnumNPData)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  Message msg;
  msg._to = "+15108580401";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580401;rn.*+151085804;npdi@homedomain.*"), hdrs, false);
}

// Test where the request URI represents a number and has NP data. The ENUM
// lookup returns a URI representing a number, so no rewrite is done
TEST_F(SCSCFTest, TestEnumReqURIwithNPData)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  Message msg;
  msg._to = "+15108580401;npdi;rn=+16";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*15108580401;rn.*+16;npdi@homedomain"), hdrs, false);
}

// Test where the request URI represents a number and has NP data. The ENUM
// lookup returns a URI representing a number, and override_npdi is on,
// so the request URI is rewritten
TEST_F(SCSCFTest, TestEnumReqURIwithNPDataOverride)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  _scscf_sproutlet->set_override_npdi(true);
  Message msg;
  msg._to = "+15108580401;npdi;rn=+16";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580401;rn.*+151085804;npdi@homedomain.*"), hdrs, false);
}

// Test where the request URI represents a number and has NP data. The ENUM
// lookup returns a URI that doesn't represent a number so the request URI
// is rewritten
TEST_F(SCSCFTest, TestEnumReqURIwithNPDataToSIP)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  URIClassifier::enforce_user_phone = true;
  Message msg;
  msg._to = "+15108580272;rn=+16";
  msg._requri = "sip:+15108580272;rn=+16@homedomain;user=phone";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580272@ut.cw-ngv.com"), hdrs, false);
}

// Test where the request URI represents a number and has NP data. The ENUM
// lookup returns a URI that doesn't represent a number so the request URI
// is rewritten
TEST_F(SCSCFTest, DISABLED_TestEnumToCIC)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  URIClassifier::enforce_user_phone = true;
  Message msg;
  msg._to = "+15108580501";
  msg._requri = "sip:+15108580501@homedomain;user=phone";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580501;cic=12345@homedomain.*"), hdrs, false);
}


// Test where the BGCF receives a SIP request URI represents a number and has NP data.
// The ENUM lookup returns a rn which the BGCF routes on.
TEST_F(SCSCFTest, TestEnumNPBGCFSIP)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _scscf_sproutlet->set_override_npdi(true);

  Message msg;
  msg._to = "+15108580401";
  msg._requri = "sip:+15108580401@homedomain;user=phone";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route", "Route: <sip:10.0.0.1:5060;transport=TCP;lr>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580401;rn.*+151085804;npdi@homedomain.*"), hdrs, false);
}

// Test where the BGCF receives a Tel request URI represents a number and has NP data.
// The ENUM lookup returns a rn which the BGCF routes on.
TEST_F(SCSCFTest, TestEnumNPBGCFTel)
{
  SCOPED_TRACE("");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _scscf_sproutlet->set_override_npdi(true);

  Message msg;
  msg._to = "+15108580401";
  msg._toscheme = "tel";
  msg._todomain = "";
  msg._requri = "tel:+15108580401";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route", "Route: <sip:10.0.0.1:5060;transport=TCP;lr>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580401;rn.*+151085804;npdi@homedomain.*"), hdrs, false);
}

// We can run with no ENUM service - in this case we expect the Request-URI to
// be unchanged (as there's no lookup which can change it) and for it to just
// be routed normally to the I-CSCF.
TEST_F(SCSCFTest, TestWithoutEnum)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "+15108580271", "homedomain", "sip:+15108580271@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:+16505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("tel:+15108580271", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:+15108580271@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:+15108580271</Identity></PublicIdentity>"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/tel%3A%2B15108580271/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  // Disable ENUM.
  _scscf_sproutlet->_enum_service = NULL;

  Message msg;
  msg._to = "+15108580271";
  msg._requri = "sip:+15108580271@homedomain;user=phone";

  // We only do ENUM on originating calls
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "Record-Route: <sip:homedomain>\nP-Asserted-Identity: <sip:+16505551000@homedomain>";
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  list<HeaderMatcher> hdrs;

  // Skip the ACK and BYE on this request by setting the last
  // parameter to false, as we're only testing Sprout functionality
  doSuccessfulFlow(msg, testing::MatchesRegex(".*+15108580271@10.114.61.213:5061;transport=tcp;.*"), hdrs, false);
}


/// Test a forked flow - setup phase.
void SCSCFTest::setupForkedFlow(SP::Message& msg)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:awwnawmaw@10.114.61.213:5061;transport=tcp;ob");
  pjsip_msg* out;

  // Send INVITE
  inject_msg(msg.get_request());
  ASSERT_EQ(4, txdata_count());

  // 100 Trying goes back
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // Collect INVITEs
  for (int i = 0; i < 3; i++)
  {
    out = current_txdata()->msg;
    ReqMatcher req("INVITE");
    req.matches(out);
    _uris.push_back(req.uri());
    _tdata[req.uri()] = pop_txdata();
  }

  EXPECT_TRUE(_tdata.find("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob") != _tdata.end());
  EXPECT_TRUE(_tdata.find("sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob") != _tdata.end());
  EXPECT_TRUE(_tdata.find("sip:awwnawmaw@10.114.61.213:5061;transport=tcp;ob") != _tdata.end());
}

TEST_F(SCSCFTest, TestForkedFlow)
{
  SCOPED_TRACE("");
  pjsip_msg* out;
  Message msg;
  setupForkedFlow(msg);
  ASSERT_EQ(3u, _tdata.size());

  // Send 183 back from one of them
  inject_msg(respond_to_txdata(_tdata[_uris[0]], 183, "early"));

  // 183 goes back
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(183, "early").matches(out);
  free_txdata();

  // Send 100 back from another one of them
  inject_msg(respond_to_txdata(_tdata[_uris[2]], 100));

  // Send 200 OK from another of them
  inject_msg(respond_to_txdata(_tdata[_uris[1]], 200, "bbb"));
  poll();
  ASSERT_EQ(3, txdata_count());

  // OK goes back
  out = current_txdata()->msg;
  RespMatcher(200, "bbb").matches(out);
  free_txdata();

  // Others are cancelled

  // Receive and respond to CANCEL for target 0
  SCOPED_TRACE("");
  out = current_txdata()->msg;
  ReqMatcher c0("CANCEL");
  c0.matches(out);
  EXPECT_THAT(c0.uri(), StrEq(_uris[0]));
  inject_msg(respond_to_current_txdata(200));

  // Receive and respond to CANCEL for target 2
  SCOPED_TRACE("");
  out = current_txdata()->msg;
  ReqMatcher c2("CANCEL");
  c2.matches(out);
  EXPECT_THAT(c2.uri(), StrEq(_uris[2]));
  inject_msg(respond_to_current_txdata(200));

  // Send 487 response from target 0
  SCOPED_TRACE("");
  inject_msg(respond_to_txdata(_tdata[_uris[0]], 487));
  ASSERT_EQ(1, txdata_count());
  // Acknowledges cancel from target 0
  ReqMatcher a0("ACK");
  a0.matches(current_txdata()->msg);
  EXPECT_THAT(a0.uri(), StrEq(_uris[0]));
  free_txdata();

  // Send 487 response from target 2
  SCOPED_TRACE("");
  inject_msg(respond_to_txdata(_tdata[_uris[2]], 487));
  ASSERT_EQ(1, txdata_count());
  // Acknowledges cancel from target 2
  ReqMatcher a2("ACK");
  a2.matches(current_txdata()->msg);
  EXPECT_THAT(a2.uri(), StrEq(_uris[2]));
  free_txdata();

  // All done!
  expect_all_tsx_done();

  // Ensure we count the forked INVITEs
  EXPECT_EQ(2, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_forked_invite_tbl)->_count);
}

TEST_F(SCSCFTest, TestForkedFlow2)
{
  SCOPED_TRACE("");
  pjsip_msg* out;
  Message msg;
  setupForkedFlow(msg);
  ASSERT_EQ(3u, _tdata.size());

  // Send 183 back from one of them
  inject_msg(respond_to_txdata(_tdata[_uris[0]], 183));

  // 183 goes back
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(183).matches(out);
  free_txdata();

  // Send 100 back from one of them
  inject_msg(respond_to_txdata(_tdata[_uris[2]], 100));

  // Send final error from another of them
  inject_msg(respond_to_txdata(_tdata[_uris[1]], 404));

  // Gets acknowledged directly by us
  ASSERT_EQ(1, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();

  // Send final success from first of them
  inject_msg(respond_to_txdata(_tdata[_uris[0]], 200, "abc"));
  poll();

  // Succeeds!
  ASSERT_EQ(2, txdata_count());

  // OK goes back
  out = current_txdata()->msg;
  RespMatcher(200, "abc").matches(out);
  free_txdata();

  // Other is cancelled
  out = current_txdata()->msg;
  ReqMatcher c2("CANCEL");
  c2.matches(out);
  EXPECT_THAT(c2.uri(), StrEq(_uris[2]));
  inject_msg(respond_to_current_txdata(200));
  free_txdata();

  // Send 487 response from target 2
  SCOPED_TRACE("");
  inject_msg(respond_to_txdata(_tdata[_uris[2]], 487));
  ASSERT_EQ(1, txdata_count());
  // Acknowledges cancel from target 2
  ReqMatcher a2("ACK");
  a2.matches(current_txdata()->msg);
  EXPECT_THAT(a2.uri(), StrEq(_uris[2]));
  free_txdata();

  // All done!
  expect_all_tsx_done();

  // Ensure we count the forked INVITEs
  EXPECT_EQ(2, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_forked_invite_tbl)->_count);
}

TEST_F(SCSCFTest, TestForkedFlow3)
{
  SCOPED_TRACE("");
  pjsip_msg* out;
  Message msg;
  setupForkedFlow(msg);
  ASSERT_EQ(3u, _tdata.size());

  // Send 183 back from one of them
  inject_msg(respond_to_txdata(_tdata[_uris[0]], 183));
  // 183 goes back
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(183).matches(out);
  free_txdata();

  // Send final error from another of them
  inject_msg(respond_to_txdata(_tdata[_uris[1]], 404));
  poll();

  // Gets acknowledged directly by us
  ASSERT_EQ(1, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();

  // Send final error from a third
  inject_msg(respond_to_txdata(_tdata[_uris[2]], 503));

  // Gets acknowledged directly by us
  ASSERT_EQ(1, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();

  // Send final failure from first of them
  inject_msg(respond_to_txdata(_tdata[_uris[0]], 301));

  // Gets acknowledged directly by us
  ASSERT_EQ(2, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();

  // "best" failure goes back
  out = current_txdata()->msg;
  RespMatcher(301).matches(out);
  free_txdata();

  // All done!
  expect_all_tsx_done();

  // Ensure we count the forked INVITEs
  EXPECT_EQ(2, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_forked_invite_tbl)->_count);
}

TEST_F(SCSCFTest, TestForkedFlow4)
{
  SCOPED_TRACE("");
  Message msg;
  setupForkedFlow(msg);
  ASSERT_EQ(3u, _tdata.size());

  // Send final error from one of them
  inject_msg(respond_to_txdata(_tdata[_uris[0]], 503));
  // Gets acknowledged directly by us
  ASSERT_EQ(1, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();

  // Send final error from another of them
  inject_msg(respond_to_txdata(_tdata[_uris[1]], 408));

  // Gets acknowledged directly by us
  ASSERT_EQ(1, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();

  // Send a CANCEL from the caller
  msg._method = "CANCEL";
  inject_msg(msg.get_request());

  // CANCEL gets OK'd
  ASSERT_EQ(1, txdata_count());
  RespMatcher(200).matches(current_txdata()->msg);
  free_txdata();

  // No CANCEL sent immediately because target 2 hasn't sent a response.
  ASSERT_EQ(0, txdata_count());

  // Send in a 100 Trying from target 2
  inject_msg(respond_to_txdata(_tdata[_uris[2]], 100));

  // Gets passed through to target 2
  ASSERT_EQ(1, txdata_count());
  ReqMatcher c2("CANCEL");
  c2.matches(current_txdata()->msg);
  EXPECT_THAT(c2.uri(), StrEq(_uris[2]));

  // Respond from target 2 to CANCEL
  inject_msg(respond_to_current_txdata(200));
  // Nothing happens yet
  ASSERT_EQ(0, txdata_count());

  // Respond from target 2 to INVITE
  SCOPED_TRACE("");
  inject_msg(respond_to_txdata(_tdata[_uris[2]], 487));
  ASSERT_EQ(2, txdata_count());

  // Acknowledges cancel from target 2
  ReqMatcher a2("ACK");
  a2.matches(current_txdata()->msg);
  EXPECT_THAT(a2.uri(), StrEq(_uris[2]));
  free_txdata();

  // Finally, pass cancel response back to initial INVITE
  ASSERT_EQ(1, txdata_count());
  RespMatcher(487).matches(current_txdata()->msg);
  free_txdata();

  // All done!
  expect_all_tsx_done();

  // Ensure we count the forked INVITEs
  EXPECT_EQ(2, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_forked_invite_tbl)->_count);
}

// Test SIP Message flows
TEST_F(SCSCFTest, TestSIPMessageSupport)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  Message msg;
  msg._via = "10.99.88.11:12345";
  pjsip_msg* out;
  pjsip_tx_data* message = NULL;

  // Send MESSAGE
  SCOPED_TRACE("MESSAGE");
  msg._method = "MESSAGE";
  inject_msg(msg.get_request(), _tp_default);
  poll();

  // MESSAGE passed on
  SCOPED_TRACE("MESSAGE (S)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("MESSAGE").matches(out));
  _tp_default->expect_target(current_txdata(), false);

   message = pop_txdata();

   // Send 200 OK back
  SCOPED_TRACE("200 OK (MESSAGE)");
  inject_msg(respond_to_txdata(message, 200), _tp_default);
  ASSERT_EQ(1, txdata_count());

  // OK goes back
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  _tp_default->expect_target(current_txdata(), true);

  free_txdata();
}

// Test that a multipart message can be parsed successfully
TEST_F(SCSCFTest, TestSimpleMultipart)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  Message msg;
  msg._content_type = "multipart/mixed;boundary=\"boundary1\"";
  msg._body = "\r\n--boundary1\r\nContent-Type: application/sdp\r\nContent-Length: 343\r\n\r\nv=0\r\no=- 3600506724 3600506724 IN IP4 888.888.888.888\r\n" \
              "s=-\r\nc=IN IP4 888.888.888.888\r\nt=0 0\r\nm=message 9 TCP/MSRP *\r\na=path:msrp://888.888.888.888:7777/1391517924073;tcp\r\n" \
              "a=setup:active\r\na=accept-types:message/cpim application/im-iscomposing+xml\r\na=accept-wrapped-types:text/plain message/imdn+xml " \
              "application/rcspushlocation+xml\r\na=sendrecv\r\n\r\n--boundary1\r\nContent-Type: message/cpim\r\nContent-Length: 300\r\n\r\nFrom: " \
              "<sip:anonymous@anonymous.invalid>\r\nTo: <sip:anonymous@anonymous.invalid>\r\nNS: imdn <urn:ietf:params:imdn>\r\nimdn.Message-ID: " \
              "Msg6rn78PUQzC\r\nDateTime: 2014-02-04T12:45:24.000Z\r\nimdn.Disposition-Notification: positive-delivery, display\r\n\r\nContent-type: " \
              "text/plain; charset=utf-8\r\n\r\nsubject\r\n\r\n--boundary1--";

  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);
}

// Test emergency registrations receive calls.
TEST_F(SCSCFTest, TestReceiveCallToEmergencyBinding)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;sos;ob");
  Message msg;

  pjsip_msg* out;

  // Send INVITE
  inject_msg(msg.get_request());
  ASSERT_EQ(3, txdata_count());

  // 100 Trying goes back
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // Collect INVITEs
  for (int i = 0; i < 2; i++)
  {
    out = current_txdata()->msg;
    ReqMatcher req("INVITE");
    req.matches(out);
    _uris.push_back(req.uri());
    _tdata[req.uri()] = pop_txdata();
  }

  EXPECT_TRUE(_tdata.find("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob") != _tdata.end());
  EXPECT_TRUE(_tdata.find("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;sos;ob") != _tdata.end());
}

// Test basic ISC (AS) flow.
TEST_F(SCSCFTest, SimpleISCMainline)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(StrEq("sip:1.2.3.4:56789;transport=UDP")));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551000@homedomain>;sescase=orig;regstate=unreg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);

  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);

  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}

// Test basic ISC (AS) flow that involves multiple responses to a single
// request.
TEST_F(SCSCFTest, ISCMultipleResponses)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  // Only expect one cal into the AS communication tracker despite receiving
  // multiple responses to the same request.
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(StrEq("sip:1.2.3.4:56789;transport=UDP")));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551000@homedomain>;sescase=orig;regstate=unreg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);

  pjsip_tx_data* txdata = pop_txdata();

  // Send a 180 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 180), 180, 2);

  // The 180 counts as the session having been setup from a stats perspective.
  // Check that the stats have been incremented accordingly.
  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);

  // Also send a 200 OK to check that the AS only gets tracked as successful
  // once.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);

  // Check that 200 OK hasn't resulted in any more session setup stats being
  // accumulated.
  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);

  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}
// Test that, if we change a SIP URI to an aliased TEL URI, it doesn't count as a retarget for
// originating-cdiv purposes.
TEST_F(SCSCFTest, ISCRetargetWithoutCdiv)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "<PublicIdentity><Identity>tel:6505551234</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("tel:6505551234", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "<PublicIdentity><Identity>tel:6505551234</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  const pj_str_t STR_NUMBER = pj_str("6505551234");
  pjsip_tel_uri* new_requri = pjsip_tel_uri_create(current_txdata()->pool);
  new_requri->number = STR_NUMBER;
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  out->line.req.uri = (pjsip_uri*)new_requri;
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
}


TEST_F(SCSCFTest, URINotIncludedInUserData)
{
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("tel:8886505551234", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // Send a terminating INVITE for a subscriber with invalid HSS data
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._todomain = "";
  msg._requri = "tel:8886505551234";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // Message is rejected with a 4xx-class response
  out = current_txdata()->msg;
  RespMatcher(480).matches(out);
  free_txdata();
}

// Test basic ISC (AS) flow.
TEST_F(SCSCFTest, SimpleISCTwoRouteHeaders)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>\r\nRoute: <sip:abcde.com>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>\r\nRoute: <sip:abcde.com>"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  free_txdata();
}

// Test handling of iFC with a malformed AS URI.
TEST_F(SCSCFTest, ISCASURIMalformed)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip::5060</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  // INVITE rejected with 502 Bad Gateway response.
  out = current_txdata()->msg;
  RespMatcher(502).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();
}

// Test handling of iFC with a AS Tel URI.
TEST_F(SCSCFTest, ISCASURITel)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>tel:1234</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  // INVITE rejected with 502 Bad Gateway response.
  out = current_txdata()->msg;
  RespMatcher(502).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();
}

// Test basic ISC (AS) flow with a single "Next" on the originating side.
TEST_F(SCSCFTest, SimpleNextOrigFlow)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>0</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>ETAOIN_SHRDLU</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:linotype.example.org</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
}


// Test basic ISC (AS) rejection flow.
TEST_F(SCSCFTest, SimpleReject)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 rejects it.
  string fresp = respond_to_txdata(current_txdata(), 404);
  free_txdata();
  inject_msg(fresp, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // 404 response goes back to bono
  SCOPED_TRACE("404");
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  msg._cseq++;
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);
}


// Test basic ISC (AS) terminating-only flow: call comes from non-local user.
TEST_F(SCSCFTest, SimpleNonLocalReject)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._fromdomain = "remote-base.mars.int";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 rejects it.
  string fresp = respond_to_txdata(current_txdata(), 404);
  free_txdata();
  inject_msg(fresp, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // 404 response goes back to bono
  SCOPED_TRACE("404");
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  msg._cseq++;
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);
}


// Test basic ISC (AS) final acceptance flow (AS sinks request).
TEST_F(SCSCFTest, SimpleAccept)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 accepts it with 200.
  string fresp = respond_to_txdata(current_txdata(), 200);
  free_txdata();
  inject_msg(fresp, &tpAS1);

  // 200 response goes back to bono
  SCOPED_TRACE("OK");
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  msg._cseq++;
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();
}


// Test basic ISC (AS) redirection flow.
TEST_F(SCSCFTest, SimpleRedirect)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 redirects it to another user on the same server.
  string fresp = respond_to_txdata(current_txdata(), 302, "", "Contact: sip:6505559876@homedomain");
  free_txdata();
  inject_msg(fresp, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // 302 response goes back to bono
  SCOPED_TRACE("Redirect");
  out = current_txdata()->msg;
  RespMatcher(302).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  EXPECT_EQ("Contact: <sip:6505559876@homedomain>", get_headers(out, "Contact"));
  msg.set_route(out);
  msg._cseq++;
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);
}


// Test DefaultHandling=TERMINATE for non-responsive AS.
TEST_F(SCSCFTest, DefaultHandlingTerminate)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>1</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  EXPECT_CALL(*_sess_term_comm_tracker, on_failure(_, HasSubstr("408")));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._fromdomain = "remote-base.mars.int";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 rejects it with a 408 error.
  string fresp = respond_to_txdata(current_txdata(), 408);
  free_txdata();
  inject_msg(fresp, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // 408 response goes back to bono
  SCOPED_TRACE("408");
  out = current_txdata()->msg;
  RespMatcher(408).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  msg._cseq++;
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);
}


// Disabled because terminated default handling is broken at the moment.
TEST_F(SCSCFTest, DISABLED_DefaultHandlingTerminateTimeout)
{
  // Register an endpoint to act as the callee.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Set up an application server for the caller. It's default handling is set
  // to session continue.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=tcp</ServerName>\n"
                                "    <DefaultHandling>1</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  EXPECT_CALL(*_sess_term_comm_tracker, on_failure(_, HasSubstr("timeout")));

  TransportFlow tpCaller(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpCallee(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.114.61.213", 5061);

  // Caller sends INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to caller
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  free_txdata();

  // Advance time without receiving a response. The application server is
  // bypassed.
  cwtest_advance_time_ms(6000);

  // 408 received at callee.
  poll();
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(408).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  // Caller ACKs error response.
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(1, txdata_count());
}


// Disabled because terminated default handling is broken at the moment.
TEST_F(SCSCFTest, DefaultHandlingTerminateDisabled)
{
  // Disable the liveness timer for session terminated ASs.
  _scscf_sproutlet->set_session_terminated_timeout(0);

  // Register an endpoint to act as the callee.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Set up an application server for the caller. It's default handling is set
  // to session continue.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=tcp</ServerName>\n"
                                "    <DefaultHandling>1</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpCaller(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpCallee(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.114.61.213", 5061);

  // Caller sends INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to caller
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  free_txdata();

  // Advance time without receiving a response. Nothing happens straight away.
  cwtest_advance_time_ms(6000);
  poll();
  ASSERT_EQ(0, txdata_count());

  // After another 26s the AS transaction times out and the call fails.
  cwtest_advance_time_ms(26000);
  poll();
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(408).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  // Caller ACKs error response.
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(0, txdata_count());
}


// Test DefaultHandling=CONTINUE for non-existent AS (where name does not resolve).
TEST_F(SCSCFTest, DefaultHandlingContinueRecordRouting)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:who@example.net");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");

 _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, HasSubstr("Transport"))).Times(2);
  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  stack_data.record_route_on_initiation_of_terminating = true;
  stack_data.record_route_on_completion_of_originating = true;
  stack_data.record_route_on_diversion = false;
  stack_data.record_route_on_every_hop = false;

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  free_txdata();

  // AS name fails to resolve, so INVITE passed on to final destination
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  EXPECT_NE("", get_headers(out, "Record-Route"));

  free_txdata();

  stack_data.record_route_on_initiation_of_terminating = false;
  stack_data.record_route_on_completion_of_originating = false;
  stack_data.record_route_on_diversion = false;
  stack_data.record_route_on_every_hop = false;
}

// Test DefaultHandling=CONTINUE for non-existent AS (where name does not resolve).
TEST_F(SCSCFTest, DefaultHandlingContinueNonExistent)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:who@example.net");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // AS name fails to resolve, so INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
}


// Test DefaultHandling=CONTINUE for non-responsive AS.
TEST_F(SCSCFTest, DefaultHandlingContinueNonResponsive)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:who@example.net");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(StrEq("sip:1.2.3.4:56789;transport=UDP"), _));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 rejects it with a 408 error.
  string fresp = respond_to_txdata(current_txdata(), 408);
  free_txdata();
  inject_msg(fresp, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
}

// Test DefaultHandling=CONTINUE for an AS that returns an error immediately.
TEST_F(SCSCFTest, DefaultHandlingContinueImmediateError)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
    _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  // This flow counts as an unsuccessful AS communication, as a 100 trying does
  // not cause an AS to be treated as responsive.
  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, HasSubstr("500")));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 immediately rejects the request with a 500 response.  This
  // gets returned to the caller because the 183 indicated the AS is live.
  std::string fresp = respond_to_txdata(current_txdata(), 500);
  inject_msg(fresp, &tpAS1);
  free_txdata();

  // ACK goes back to AS1
  ASSERT_EQ(2, txdata_count());
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  free_txdata();
}

// Test DefaultHandling=CONTINUE for an AS that returns 100 Trying followed by
// an error.
TEST_F(SCSCFTest, DefaultHandlingContinue100ThenError)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
    _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  // This flow counts as an unsuccessful AS communication, as a 100 trying does
  // not cause an AS to be treated as responsive.
  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, _));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 sends a 100 Trying to indicate it is processing the
  // request.  This does NOT disable the default handling.
  //
  // Save off the INVITE TX data so we can build a final response later on.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 now rejects the request with a 500 response.  This gets
  // returned to the caller because the 183 indicated the AS is live.
  fresp = respond_to_txdata(current_txdata(), 500);
  inject_msg(fresp, &tpAS1);
  free_txdata();

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  free_txdata();
}

// Test DefaultHandling=CONTINUE for a responsive AS that returns an error.
TEST_F(SCSCFTest, DefaultHandlingContinue1xxThenError)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
    _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  // This flow counts as a successful AS communication, as it sent back a 1xx
  // response.
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(_));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 sends a 183 Session Progress to indicate it is processing the
  // request.  This will disable the default handling.
  //
  // Save off the INVITE TX data so we can build a final response later on.
  pjsip_tx_data* invite_tx_data = pop_txdata();
  string fresp = respond_to_txdata(invite_tx_data, 183);
  inject_msg(fresp, &tpAS1);

  // 183 flows back to Bono.
  SCOPED_TRACE("183");
  out = current_txdata()->msg;
  RespMatcher(183).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  // ---------- AS1 now rejects the request with a 500 response.  This gets
  // returned to the caller because the 183 indicated the AS is live.
  fresp = respond_to_txdata(invite_tx_data, 500);
  pjsip_tx_data_dec_ref(invite_tx_data); invite_tx_data = NULL;
  inject_msg(fresp, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // 500 response goes back to bono
  SCOPED_TRACE("500");
  out = current_txdata()->msg;
  RespMatcher(500).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._cseq++;
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);
}


// Test DefaultHandling=CONTINUE for a responsive AS that passes the INVITE
// back tot he S-CSCF but then returns an error.
TEST_F(SCSCFTest, DefaultHandlingContinueInviteReturnedThenError)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
    _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  // This flow is classed as a successful AS flow, as the AS will pass the
  // INVITE back to the S-CSCF which indicates it is responsive.
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(_));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string resp_100 = respond_to_txdata(current_txdata(), 100);
  inject_msg(resp_100, &tpAS1);

  // We are going to send a 500 response to this request later on in the test
  // case. Build this now, as it means we can mutate the INVITE for sending
  // back to sprout.
  string resp_500 = respond_to_txdata(current_txdata(), 500);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();

  // ---------- AS1 now rejects the request with a 500 response.  The AS is not
  // bypassed as the INVITE it sent back to sprout indicates that it is live.
  inject_msg(resp_500, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // 500 response goes back to bono
  SCOPED_TRACE("500");
  out = current_txdata()->msg;
  RespMatcher(500).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._cseq++;
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);

  // Check there are no outstanding messages - this confirms sprout did not
  // create a fork to bypass the AS.
  ASSERT_EQ(0, txdata_count());
}


TEST_F(SCSCFTest, DefaultHandlingContinueTimeout)
{
  // Register an endpoint to act as the callee.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Set up an application server for the caller. It's default handling is set
  // to session continue.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=tcp</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, HasSubstr("timeout")));

  TransportFlow tpCaller(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpCallee(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.114.61.213", 5061);

  // Caller sends INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to caller
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  free_txdata();

  // Advance time without receiving a response. The application server is
  // bypassed.
  cwtest_advance_time_ms(3000);

  // INVITE is sent to the callee.
  poll();
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));
  tpCallee.expect_target(current_txdata(), true);

  // Callee sends 200 OK.
  inject_msg(respond_to_txdata(current_txdata(), 200, "", ""), &tpCallee);
  free_txdata();

  // 200 OK received at callee.
  poll();
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();
}

TEST_F(SCSCFTest, DefaultHandlingContinueDisabled)
{
  // Set the session continue timer to 0 to disable it.
  _scscf_sproutlet->set_session_continued_timeout(0);

  // Register an endpoint to act as the callee.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Set up an application server for the caller. It's default handling is set
  // to session continue.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=tcp</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, _));

  TransportFlow tpCaller(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpCallee(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.114.61.213", 5061);

  // Caller sends INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to caller
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  free_txdata();

  // Advance time without receiving a response. The liveness time is not
  // running which means the AS is not immediately bypassed.
  cwtest_advance_time_ms(3000);
  poll();
  ASSERT_EQ(0, txdata_count());

  // After another 29s the AS transaction times out and the INVITE is sent to
  // the callee.
  cwtest_advance_time_ms(29000);
  poll();

  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));
  tpCallee.expect_target(current_txdata(), true);

  // Callee sends 200 OK.
  inject_msg(respond_to_txdata(current_txdata(), 200, "", ""), &tpCallee);
  free_txdata();

  // 200 OK received at callee.
  poll();
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();
}


// Test DefaultHandling attribute missing.
TEST_F(SCSCFTest, DefaultHandlingMissing)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:who@example.net");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=UDP</ServerName>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // AS name fails to resolve, so INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
}


// Test DefaultHandling attribute malformed.
TEST_F(SCSCFTest, DefaultHandlingMalformed)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:who@example.net");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>frog</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // AS name fails to resolve, so INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r2.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
}

// Test DefaultHandling=CONTINUE for non-existent AS (where name does not resolve).
//
// This test configures an AS for the originating subscriber, and checks that
// the S-CSCF still record-routes itself correctly when the AS fails and is
// bypassed.
TEST_F(SCSCFTest, DefaultHandlingContinueNonExistentRRTest)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:who@example.net");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // AS name fails to resolve, so INVITE passed on to final destination
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // The S-CSCF should record-route itself for both originating and terminating
  // billing.
  EXPECT_THAT(get_headers(out, "Record-Route"),
              MatchesRegex("Record-Route:.*billing-role=charge-term.*"
                           "Record-Route:.*billing-role=charge-orig.*"));

  free_txdata();
}

// Test DefaultHandling=CONTINUE for an unresponsive AS.
//
// This test configures an AS for the originating subscriber, and checks that
// the S-CSCF still record-routes itself correctly when the AS times out and is
// bypassed.
TEST_F(SCSCFTest, DefaultHandlingContinueTimeoutRRTest)
{
  // Register an endpoint to act as the callee.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Set up an application server for the caller. It's default handling is set
  // to session continue.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=tcp</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, HasSubstr("timeout")));

  TransportFlow tpCaller(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpCallee(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.114.61.213", 5061);

  // Caller sends INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to caller
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  free_txdata();

  // Advance time without receiving a response. The application server is
  // bypassed.
  cwtest_advance_time_ms(3000);

  // INVITE is sent to the callee.
  poll();
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));
  tpCallee.expect_target(current_txdata(), true);

  EXPECT_THAT(get_headers(out, "Record-Route"),
              MatchesRegex("Record-Route:.*billing-role=charge-term.*"
                           "Record-Route:.*billing-role=charge-orig.*"));

  // Callee sends 200 OK.
  inject_msg(respond_to_txdata(current_txdata(), 200, "", ""), &tpCallee);
  free_txdata();

  // 200 OK received at callee.
  poll();
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();
}

// Test DefaultHandling=CONTINUE for non-existent AS.
//
// This test configures two ASs for the originating subscriber, and checks that
// the S-CSCF still record-routes itself correctly when the first AS fails, is
// bypassed, and the request is routed to the second AS.
TEST_F(SCSCFTest, DefaultHandlingContinueFirstAsFailsRRTest)
{
  // Register an endpoint to act as the callee.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Set up an application server for the caller. It's default handling is set
  // to session continue.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=tcp</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>2</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, HasSubstr("Transport error")));

  TransportFlow tpCaller(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpCallee(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.114.61.213", 5061);

  // Caller sends INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to caller
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // The first AS fails to resolve so the INVITE is passed on to AS2
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  free_txdata();

  // The S-CSCF should have record-routed itself at the start of originating
  // processing, and this should be reflected in the request sent to the second
  // AS.
  EXPECT_THAT(get_headers(out, "Record-Route"),
              MatchesRegex("Record-Route:.*billing-role=charge-orig.*"));
  free_txdata();
}

// Test DefaultHandling=CONTINUE for non-existent AS.
//
// This test configures two ASs for the terminating subscriber, and checks that
// the S-CSCF still record-routes itself correctly when the first AS fails, is
// bypassed, and the request is routed to the second AS.
//
// This test configured record routing at the start of terminating processing,
// so we can check the terminating Record-Route is preserved when bypassing the
// AS.
TEST_F(SCSCFTest, DefaultHandlingContinueFirstTermAsFailsRRTest)
{
  // Register an endpoint to act as the callee.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // Set up an application server for the caller. It's default handling is set
  // to session continue.
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:ne-as:56789;transport=tcp</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>2</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  EXPECT_CALL(*_sess_cont_comm_tracker, on_failure(_, HasSubstr("Transport error")));

  TransportFlow tpCaller(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpCallee(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.114.61.213", 5061);

  bool old_rr_on_comp_of_orig = stack_data.record_route_on_completion_of_originating;
  bool old_rr_on_init_of_term = stack_data.record_route_on_initiation_of_terminating;
  stack_data.record_route_on_initiation_of_terminating = true;
  stack_data.record_route_on_completion_of_originating = true;

  // Caller sends INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpCaller);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to caller
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpCaller.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // The first AS fails to resolve so the INVITE is passed on to AS2
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  free_txdata();

  // The S-CSCF should have record-routed itself at the start of originating
  // processing, end of originating, and start of terminating. However:
  // -  We don't bill at the start of terminating processing, so the top route
  //    header should indicate no billing.
  // -  There was only one signaling hop on the originating side, so there
  //    should only be one RR header from originating processing, which should
  //    indicate originating billing).
  EXPECT_THAT(get_headers(out, "Record-Route"),
              MatchesRegex("Record-Route:.*billing-role=charge-none.*"
                           "Record-Route:.*billing-role=charge-orig.*"));
  free_txdata();

  stack_data.record_route_on_initiation_of_terminating = old_rr_on_init_of_term;
  stack_data.record_route_on_completion_of_originating = old_rr_on_comp_of_orig;
}

// Test that when Sprout is configured to Record-Route itself only at
// the start and end of all processing, it does.
TEST_F(SCSCFTest, RecordRoutingTest)
{
  // Expect 2 Record-Routes:
  // - on start of originating handling
  // - AS1's Record-Route
  // - AS2's Record-Route
  // - AS3's Record-Route
  // - AS4's Record-Route
  // - on end of terminating handling

  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  doFourAppServerFlow("Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-term>\r\n"
                      "Record-Route: <sip:6.2.3.4>\r\n"
                      "Record-Route: <sip:5.2.3.4>\r\n"
                      "Record-Route: <sip:4.2.3.4>\r\n"
                      "Record-Route: <sip:1.2.3.4>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-orig>", true);
  free_txdata();
}

// Test that when Sprout is configured to Record-Route itself at
// the start and end of terminating and originating processing, it does.
TEST_F(SCSCFTest, RecordRoutingTestStartAndEnd)
{
  stack_data.record_route_on_completion_of_originating = true;
  stack_data.record_route_on_initiation_of_terminating = true;

  // Expect 2 Record-Routes:
  // - on start of originating handling
  // - AS1's Record-Route
  // - AS2's Record-Route
  // - on end of originating handling/on start of terminating handling
  // (collapsed together as they're identical)
  // - AS3's Record-Route
  // - AS4's Record-Route
  // - on end of terminating handling

  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  doFourAppServerFlow("Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-term>\r\n"
                      "Record-Route: <sip:6.2.3.4>\r\n"
                      "Record-Route: <sip:5.2.3.4>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:4.2.3.4>\r\n"
                      "Record-Route: <sip:1.2.3.4>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-orig>", true);
  stack_data.record_route_on_completion_of_originating = false;
  stack_data.record_route_on_initiation_of_terminating = false;
}


// Test that when Sprout is configured to Record-Route itself on each
// hop, it does.
TEST_F(SCSCFTest, RecordRoutingTestEachHop)
{
  // Simulate record-routing model 3, which sets all the record-routing flags.
  stack_data.record_route_on_initiation_of_terminating = true;
  stack_data.record_route_on_completion_of_originating = true;
  stack_data.record_route_on_diversion = true;
  stack_data.record_route_on_every_hop = true;

  // Expect 9 Record-Routes:
  // - between the endpoint and AS1
  // - AS1's Record-Route
  // - between AS1 and AS2
  // - AS2's Record-Route
  // - between AS2 and AS3
  // - AS3's Record-Route
  // - between AS3 and AS4
  // - AS4's Record-Route
  // - between AS4 and the endpoint

  // In reality we'd expect 10 (instead of having one between AS2 and
  // AS3, we'd have two - one for conclusion of originating processing
  // and one for initiation of terminating processing) but we don't
  // split originating and terminating handling like that yet.
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  doFourAppServerFlow("Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-term>\r\n"
                      "Record-Route: <sip:6.2.3.4>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:5.2.3.4>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:4.2.3.4>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:1.2.3.4>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-orig>", true);

  stack_data.record_route_on_initiation_of_terminating = false;
  stack_data.record_route_on_completion_of_originating = false;
  stack_data.record_route_on_diversion = false;
  stack_data.record_route_on_every_hop = false;
}

// Test that Sprout only adds a single Record-Route if none of the Ases
// Record-Route themselves.
TEST_F(SCSCFTest, RecordRoutingTestCollapse)
{
  // Expect 1 Record-Route
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  doFourAppServerFlow("Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-term>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-orig>", false);
}

// Test that even when Sprout is configured to Record-Route itself on each
// hop, it only adds a single Record-Route if none of the Ases
// Record-Route themselves.
TEST_F(SCSCFTest, RecordRoutingTestCollapseEveryHop)
{
  stack_data.record_route_on_every_hop = true;
  // Expect 1 Record-Route
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  doFourAppServerFlow("Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-term>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-none>\r\n"
                      "Record-Route: <sip:scscf.sprout.homedomain:5058;transport=TCP;lr;billing-role=charge-orig>", false);
  stack_data.record_route_on_every_hop = false;
}

// Test AS-originated flow.
void SCSCFTest::doAsOriginated(Message& msg, bool expect_orig)
{
  doAsOriginated(msg.get_request(), expect_orig);
}

void SCSCFTest::doAsOriginated(const std::string& msg, bool expect_orig)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                  "<InitialFilterCriteria>"
                                    "<Priority>1</Priority>"
                                    "<TriggerPoint>"
                                    "<ConditionTypeCNF>0</ConditionTypeCNF>"
                                    "<SPT>"
                                      "<ConditionNegated>0</ConditionNegated>"
                                      "<Group>0</Group>"
                                      "<Method>INVITE</Method>"
                                      "<Extension></Extension>"
                                    "</SPT>"
                                  "</TriggerPoint>"
                                  "<ApplicationServer>"
                                    "<ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>"
                                    "<DefaultHandling>0</DefaultHandling>"
                                  "</ApplicationServer>"
                                  "</InitialFilterCriteria>"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                  "<InitialFilterCriteria>"
                                    "<Priority>0</Priority>"
                                    "<TriggerPoint>"
                                    "<ConditionTypeCNF>0</ConditionTypeCNF>"
                                    "<SPT>"
                                      "<ConditionNegated>0</ConditionNegated>"
                                      "<Group>0</Group>"
                                      "<Method>INVITE</Method>"
                                      "<Extension></Extension>"
                                    "</SPT>"
                                  "</TriggerPoint>"
                                  "<ApplicationServer>"
                                    "<ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>"
                                    "<DefaultHandling>0</DefaultHandling>"
                                  "</ApplicationServer>"
                                  "</InitialFilterCriteria>"
                                "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS0(TransportFlow::Protocol::UDP, stack_data.scscf_port, "6.2.3.4", 56786);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send spontaneous INVITE from AS0.
  inject_msg(msg, &tpAS0);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to AS0
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS0.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  ReqMatcher r1("INVITE");
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = NULL;

  if (expect_orig)
  {
    // INVITE passed on to AS1
    SCOPED_TRACE("INVITE (S)");
    out = current_txdata()->msg;
    ASSERT_NO_FATAL_FAILURE(r1.matches(out));

    tpAS1.expect_target(current_txdata(), false);
    EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
    EXPECT_THAT(get_headers(out, "Route"),
                testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));

    // ---------- AS1 sends a 100 Trying to indicate it has received the request.
    string fresp1 = respond_to_txdata(current_txdata(), 100);
    inject_msg(fresp1, &tpAS1);

    // ---------- AS1 turns it around (acting as proxy)
    hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
    if (hdr)
    {
      pj_list_erase(hdr);
    }
    inject_msg(out, &tpAS1);
    free_txdata();

    // 100 Trying goes back to AS1
    out = current_txdata()->msg;
    RespMatcher(100).matches(out);
    tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
    free_txdata();
  }

  // INVITE passed on to AS2
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS2.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp2 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp2, &tpAS1);

  // ---------- AS2 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS2);
  free_txdata();

  // 100 Trying goes back to AS2
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS2.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (Z)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // Inject succesful responses to finish up the flow
  inject_msg(respond_to_current_txdata(200));
  inject_msg(respond_to_current_txdata(200));
  inject_msg(respond_to_current_txdata(200));
}


// Test AS-originated flow - orig.
TEST_F(SCSCFTest, AsOriginatedOrig)
{
  // ---------- Send spontaneous INVITE from AS0, marked as originating-handling-required.
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
//  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";

  SCOPED_TRACE("orig");
  doAsOriginated(msg, true);

  // This is an originating call so we track a session setup time regardless of
  // the fact that it is initiated by an app server.
  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}


// Test AS-originated flow - term.
TEST_F(SCSCFTest, AsOriginatedTerm)
{
  // ---------- Send spontaneous INVITE from AS0, marked as terminating-handling-only.
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
//  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";

  msg._method = "INVITE";

  SCOPED_TRACE("term");
  doAsOriginated(msg, false);
}


// Test call-diversion AS flow.
TEST_F(SCSCFTest, Cdiv)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:wuntootree@10.14.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505555678", "homedomain", "sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>4</SessionCase>  <!-- originating-cdiv -->
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  _hss_connection->set_result("/impu/sip%3A6505555678%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;sescase=term;regstate=reg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // ---------- AS1 turns it around (acting as routing B2BUA by changing the target)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  ((pjsip_sip_uri*)out->line.req.uri)->user = pj_str("6505555678");
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS2 (as originating AS for Bob)
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS2.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505555678@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));

  // As the session case is "Originating_CDIV" we want to include the
  // "orig-div" header field parameter with just a name and no value
  // as specified in 3GPP TS 24.229.
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;orig-cdiv"));

  // ---------- AS2 sends a 100 Trying to indicate it has received the request.
  string fresp2 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp2, &tpAS2);

  // ---------- AS2 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS2);
  free_txdata();

  // 100 Trying goes back to AS2
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS2.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}


// Test call-diversion AS flow where the AS diverts to a different domain.
TEST_F(SCSCFTest, CdivToDifferentDomain)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:wuntootree@10.14.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505555678", "homedomain", "sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                      <ConditionTypeCNF>0</ConditionTypeCNF>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <SessionCase>4</SessionCase>  <!-- originating-cdiv -->
                                        <Extension></Extension>
                                      </SPT>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <Method>INVITE</Method>
                                        <Extension></Extension>
                                      </SPT>
                                    </TriggerPoint>
                                    <ApplicationServer>
                                      <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                      <DefaultHandling>0</DefaultHandling>
                                    </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                      <ConditionTypeCNF>0</ConditionTypeCNF>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <Method>INVITE</Method>
                                        <Extension></Extension>
                                      </SPT>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                        <Extension></Extension>
                                      </SPT>
                                    </TriggerPoint>
                                    <ApplicationServer>
                                      <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                      <DefaultHandling>0</DefaultHandling>
                                    </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // ---------- AS1 turns it around (acting as routing B2BUA by changing the target)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }

  // Re-target the request to a new user. Use the domain "newdomain" as this
  // will be routed off net by the BGCF.
  ((pjsip_sip_uri*)out->line.req.uri)->user = pj_str("newuser");
  ((pjsip_sip_uri*)out->line.req.uri)->host = pj_str("domainvalid");
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS2 (as originating AS for Bob)
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS2.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:newuser@domainvalid", r1.uri());

  // ---------- AS2 sends a 100 Trying to indicate it has received the request.
  string fresp2 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp2, &tpAS2);

  // ---------- AS2 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS2);
  free_txdata();

  // 100 Trying goes back to AS2
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS2.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:newuser@domainvalid", r1.uri());
  // This route header is determined from the BGCF config.
  EXPECT_EQ("Route: <sip:10.0.0.1:5060;transport=TCP;lr>", get_headers(out, "Route"));

  free_txdata();
}

// Test that ENUM lookups and appropriate URI translation is done before any terminating services are applied.
TEST_F(SCSCFTest, BothEndsWithEnumRewrite)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
    _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                               R"(<IMSSubscription><ServiceProfile>
                                  <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  URIClassifier::enforce_global = false;
  URIClassifier::enforce_user_phone = false;

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "1115551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:1115551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // These fields of the message will only be filled in correctly if we have
  // done an ENUM lookup before applying terminating services, and correctly
  // recognised that "1115551234" is "6505551234".

  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;sescase=term;regstate=reg"));

  free_txdata();
}

// Test that ENUM lookups are not done if we are only doing
// terminating processing.
TEST_F(SCSCFTest, TerminatingWithNoEnumRewrite)
{
  register_uri(_sdm, _hss_connection, "1115551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
    _hss_connection->set_impu_result("sip:1115551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                  <PublicIdentity><Identity>sip:1115551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "1115551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._requri = "sip:1115551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // These fields of the message will only be filled in correctly if we have
  // not done an ENUM lookup before applying terminating services (as
  // ENUM is only applied when originating)

  EXPECT_EQ("sip:1115551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:1115551234@homedomain>;sescase=term;regstate=reg"));

  free_txdata();
}


// Test call-diversion AS flow, where MMTEL does the diversion.
TEST_F(SCSCFTest, MmtelCdiv)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  register_uri(_sdm, _hss_connection, "6505555678", "homedomain", "sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>4</SessionCase>  <!-- originating-cdiv -->
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505551234@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="false" />
                            <originating-identity-presentation-restriction active="false">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="true">
                              <NoReplyTimer>19</NoReplyTimer>"
                                <cp:ruleset>
                                  <cp:rule id="rule1">
                                    <cp:conditions/>
                                    <cp:actions><forward-to><target>sip:6505555678@homedomain</target></forward-to></cp:actions>
                                  </cp:rule>
                                </cp:ruleset>
                              </communication-diversion>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                          </simservs>)");  // "
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  _hss_connection->set_result("/impu/sip%3A6505555678%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(3, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE goes to MMTEL as terminating AS for Bob, and is redirected to 6505555678.
  ReqMatcher r1("INVITE");
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr;

  // 181 Call is being forwarded goes back to bono
  out = current_txdata()->msg;
  RespMatcher(181).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS2 (as originating AS for Bob)
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS2.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505555678@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;orig-cdiv"));
  EXPECT_THAT(get_headers(out, "History-Info"),
              testing::MatchesRegex("History-Info: <sip:6505551234@homedomain;Reason=SIP%3[bB]cause%3[dD]480%3[bB]text%3[dD]%22Temporarily%20Unavailable%22>;index=1\r\nHistory-Info: <sip:6505555678@homedomain>;index=1.1"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS2);

  // ---------- AS2 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS2);
  free_txdata();

  // 100 Trying goes back to AS2
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS2.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));
  EXPECT_THAT(get_headers(out, "History-Info"),
              testing::MatchesRegex("History-Info: <sip:6505551234@homedomain;Reason=SIP%3[bB]cause%3[dD]480%3[bB]text%3[dD]%22Temporarily%20Unavailable%22>;index=1\r\nHistory-Info: <sip:6505555678@homedomain>;index=1.1"));

  free_txdata();
}


// Test call-diversion AS flow, where MMTEL does the diversion - twice.
TEST_F(SCSCFTest, MmtelDoubleCdiv)
{
  register_uri(_sdm, _hss_connection, "6505559012", "homedomain", "sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", "UNREGISTERED",
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>2</SessionCase>  <!-- terminating-unregistered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505551234@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="false" />
                            <originating-identity-presentation-restriction active="false">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="true">
                              <NoReplyTimer>19</NoReplyTimer>"
                                <cp:ruleset>
                                  <cp:rule id="rule1">
                                    <cp:conditions/>
                                    <cp:actions><forward-to><target>sip:6505555678@homedomain</target></forward-to></cp:actions>
                                  </cp:rule>
                                </cp:ruleset>
                              </communication-diversion>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                          </simservs>)");  // "
  _hss_connection->set_impu_result("sip:6505555678@homedomain", "call", "UNREGISTERED",
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505555678@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>4</SessionCase>  <!-- originating-cdiv -->
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>2</SessionCase>  <!-- terminating-unregistered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505555678@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="false" />
                            <originating-identity-presentation-restriction active="false">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="true">
                              <NoReplyTimer>19</NoReplyTimer>"
                                <cp:ruleset>
                                  <cp:rule id="rule1">
                                    <cp:conditions/>
                                    <cp:actions><forward-to><target>sip:6505559012@homedomain</target></forward-to></cp:actions>
                                  </cp:rule>
                                </cp:ruleset>
                              </communication-diversion>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                          </simservs>)");  // "
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  _hss_connection->set_result("/impu/sip%3A6505555678%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  _hss_connection->set_result("/impu/sip%3A6505559012%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(4, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE goes to MMTEL as terminating AS for Bob, and is redirected to 6505555678.
  ReqMatcher r1("INVITE");
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr;

  // 181 Call is being forwarded goes back to bono
  out = current_txdata()->msg;
  RespMatcher(181).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // Now INVITE is redirected to 6505559012

  // 181 Call is being forwarded goes back to bono
  out = current_txdata()->msg;
  RespMatcher(181).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS2 (as originating AS for Bob)
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS2.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505559012@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505555678@homedomain>;orig-cdiv"));
  EXPECT_THAT(get_headers(out, "History-Info"),
              testing::MatchesRegex("History-Info: <sip:6505551234@homedomain;Reason=SIP%3[bB]cause%3[dD]480%3[bB]text%3[dD]%22Temporarily%20Unavailable%22>;index=1\r\nHistory-Info: <sip:6505555678@homedomain;Reason=SIP%3[bB]cause%3[dD]480%3[bB]text%3[dD]%22Temporarily%20Unavailable%22>;index=1.1\r\nHistory-Info: <sip:6505559012@homedomain>;index=1.1.1"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS2);

  // ---------- AS2 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS2);
  free_txdata();

  // 100 Trying goes back to AS2
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS2.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:andunnuvvawun@10.114.61.214:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  free_txdata();
}


// Test attempted AS chain link after chain has expired.
TEST_F(SCSCFTest, ExpiredChain)
{
  register_uri(_sdm, _hss_connection, "6505551000", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));

  // ---------- AS1 gives final response, ending the transaction.
  string fresp = respond_to_txdata(current_txdata(), 404);
  pjsip_msg* saved = pop_txdata()->msg;
  inject_msg(fresp, &tpAS1);

  // ACK goes back to AS1
  SCOPED_TRACE("ACK");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("ACK").matches(out));
  free_txdata();

  // 404 response goes back to bono
  SCOPED_TRACE("404");
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  msg._cseq++;
  free_txdata();

  // ---------- Send ACK from bono
  SCOPED_TRACE("ACK");
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);

  // Allow time to pass, so the initial Sprout UAS transaction moves
  // from Completed to Terminated to Destroyed.  32s is the default
  // timeout. This causes the ODI token to expire.
  cwtest_advance_time_ms(33000L);
  poll();

  // ---------- AS1 attempts to turn the message around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(saved, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }

  char buf[65535];
  pj_ssize_t len = pjsip_msg_print(saved, buf, sizeof(buf));
  doAsOriginated(string(buf, len), true);
}

// Test a simple MMTEL flow.
TEST_F(SCSCFTest, MmtelFlow)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505551000@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="true" />
                            <originating-identity-presentation-restriction active="true">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="false"/>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                          </simservs>)");  // "
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // Call should pass through MMTEL AS, and then proceed. This should
  // add a privacy header.

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_EQ("Privacy: id; header; user", get_headers(out, "Privacy"));

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));
  EXPECT_EQ("Privacy: id; header; user", get_headers(out, "Privacy"));

  free_txdata();
}

/// Test MMTEL-then-external-AS flows (both orig and term).
//
// Flow:
//
// * 6505551000 calls 6505551234
// * 6505551000 originating:
//     * MMTEL is invoked, applying privacy
//     * external AS1 (1.2.3.4:56789) is invoked
// * 6505551234 terminating:
//     * MMTEL is invoked, applying privacy
//     * external AS2 (5.2.3.4:56787) is invoked
// * call reaches registered contact for 6505551234.
//
TEST_F(SCSCFTest, MmtelThenExternal)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505551000@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="true" />
                            <originating-identity-presentation-restriction active="true">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="false"/>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                       </simservs>)");  // "
    _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505551234@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="true" />
                            <originating-identity-presentation-restriction active="true">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="false"/>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                          </simservs>)");  // "
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // Call should pass through MMTEL AS, and then proceed. This should
  // add a privacy header.

  // INVITE passed on to AS1 (as originating).
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_EQ("Privacy: id; header; user", get_headers(out, "Privacy"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551000@homedomain>;sescase=orig;regstate=unreg"));

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // Call should pass through MMTEL AS, and then proceed. This should
  // do nothing.

  // INVITE passed on to AS2 (as terminating).
  SCOPED_TRACE("INVITE (S2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS2.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;sescase=term;regstate=reg"));

  // ---------- AS2 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS2);
  free_txdata();

  // 100 Trying goes back to AS2
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS2.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));
  EXPECT_EQ("Privacy: id; header; user", get_headers(out, "Privacy"));

  free_txdata();
}


/// Test multiple-MMTEL flow.
// Flow:
//
// * 6505551000 calls 6505551234
// * 6505551000 originating:
//     * MMTEL is invoked, applying privacy
//     * MMTEL is invoked, applying privacy
// * 6505551234 terminating:
//     * MMTEL is invoked, applying privacy
//     * MMTEL is invoked, applying privacy
//     * external AS1 (5.2.3.4:56787) is invoked
// * call reaches registered contact for 6505551234.
//
TEST_F(SCSCFTest, MultipleMmtelFlow)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505551000@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="true" />
                            <originating-identity-presentation-restriction active="true">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="false"/>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                          </simservs>)");  // "
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:mmtel.homedomain</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>3</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _xdm_connection->put("sip:6505551234@homedomain",
                       R"(<?xml version="1.0" encoding="UTF-8"?>
                          <simservs xmlns="http://uri.etsi.org/ngn/params/xml/simservs/xcap" xmlns:cp="urn:ietf:params:xml:ns:common-policy">
                            <originating-identity-presentation active="true" />
                            <originating-identity-presentation-restriction active="true">
                              <default-behaviour>presentation-restricted</default-behaviour>
                            </originating-identity-presentation-restriction>
                            <communication-diversion active="false"/>
                            <incoming-communication-barring active="false"/>
                            <outgoing-communication-barring active="false"/>
                          </simservs>)");  // "
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // Call should pass through MMTEL AS four times (!), and then
  // proceed. This should add a privacy header.

  // INVITE passed on to AS1
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_EQ("Privacy: id; header; user", get_headers(out, "Privacy"));

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));
  EXPECT_EQ("Privacy: id; header; user", get_headers(out, "Privacy"));

  free_txdata();
}


// Test basic ISC (AS) OPTIONS final acceptance flow (AS sinks request).
TEST_F(SCSCFTest, SimpleOptionsAccept)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>OPTIONS</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send OPTIONS
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "OPTIONS";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(1, txdata_count());

  // INVITE passed on to AS1
  SCOPED_TRACE("OPTIONS (S)");
  pjsip_msg* out = current_txdata()->msg;
  ReqMatcher r1("OPTIONS");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));

  // ---------- AS1 accepts it with 200.
  string fresp = respond_to_txdata(current_txdata(), 200);
  free_txdata();
  inject_msg(fresp, &tpAS1);

  // 200 response goes back to bono
  SCOPED_TRACE("OK");
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  msg._cseq++;
  free_txdata();
}


// Test terminating call-diversion AS flow to external URI.
// Repros https://github.com/Metaswitch/sprout/issues/519.
TEST_F(SCSCFTest, TerminatingDiversionExternal)
{
  register_uri(_sdm, _hss_connection, "6505501234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505501234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505501234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505501234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  TransportFlow tpBono(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpExternal(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.9.8.7", 5060);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345";
  msg._to = "6505501234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505501234@homedomain";
  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505501234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505501234@homedomain>;sescase=term;regstate=reg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS);

  // ---------- AS1 turns it around
  // (acting as routing B2BUA by adding a Via, removing the top Route and changing the target)
  const pj_str_t STR_VIA = pj_str("Via");
  pjsip_via_hdr* via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(current_txdata()->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK1234567890");
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  ((pjsip_sip_uri*)out->line.req.uri)->host = pj_str("ut.cw-ngv.com");
  inject_msg(out, &tpAS);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed externally
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpExternal.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505501234@ut.cw-ngv.com", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // ---------- Externally accepted with 200.
  string fresp = respond_to_txdata(current_txdata(), 200);
  free_txdata();
  inject_msg(fresp, &tpExternal);

  // 200 OK goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);

  // ---------- AS1 forwards 200 (stripping via)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 200 OK goes back to bono
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}


// Test originating AS handling for request to external URI.
TEST_F(SCSCFTest, OriginatingExternal)
{
  register_uri(_sdm, _hss_connection, "6505501234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>0</SessionCase>  <!-- originating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505501234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  TransportFlow tpBono(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpExternal(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.9.8.7", 5060);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345";
  msg._to = "6505501234@ut.cw-ngv.com";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505501234@ut.cw-ngv.com";
  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as originating AS for Alice)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505501234@ut.cw-ngv.com", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551000@homedomain>;sescase=orig;regstate=reg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS);

  // ---------- AS1 turns it around
  // (acting as routing B2BUA by adding a Via, removing the top Route and changing the target)
  const pj_str_t STR_VIA = pj_str("Via");
  pjsip_via_hdr* via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(current_txdata()->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK1234567890");
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed externally
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpExternal.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505501234@ut.cw-ngv.com", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // ---------- Externally accepted with 200.
  string fresp = respond_to_txdata(current_txdata(), 200);
  free_txdata();
  inject_msg(fresp, &tpExternal);

  // 200 OK goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);

  // ---------- AS1 forwards 200 (stripping via)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 200 OK goes back to bono
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}


// Test local call with both originating and terminating ASs.
TEST_F(SCSCFTest, OriginatingTerminatingAS)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";
  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as originating AS for 6505551000)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551000@homedomain>;sescase=orig;regstate=reg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS);

  // ---------- AS1 turns it around
  // (acting as routing B2BUA by adding a Via, and removing the top Route.)
  const pj_str_t STR_VIA = pj_str("Via");
  pjsip_via_hdr* via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(current_txdata()->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK1234567890");
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for 6505551234)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  r1 = ReqMatcher("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;sescase=term;regstate=reg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp2 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp2, &tpAS);

  // ---------- AS1 turns it around
  // (acting as routing B2BUA by adding a Via, and removing the top Route.)
  via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(current_txdata()->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK1234567891"); // Must differ from previous branch
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed to terminating UE
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  string fresp = respond_to_txdata(current_txdata(), 200);
  free_txdata();
  inject_msg(fresp, &tpBono);

  // 200 OK goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);

  // ---------- AS1 forwards 200 (stripping via)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 200 OK goes back to AS1 (terminating)
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);

  // ---------- AS1 forwards 200 (stripping via)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 200 OK goes back to bono
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}


// Test local call with both originating and terminating ASs where terminating UE doesn't respond.
TEST_F(SCSCFTest, OriginatingTerminatingASTimeout)
{
  TransportFlow tpBono(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=TCP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=TCP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345";
  msg._branch = "1111111111";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";
  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as originating AS for 6505551000)
  SCOPED_TRACE("INVITE (S)");
  pjsip_tx_data* invite_txdata = pop_txdata();
  out = invite_txdata->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpAS.expect_target(invite_txdata, false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=TCP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=TCP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551000@homedomain>;sescase=orig;regstate=reg"));

  // AS1 sends an immediate 100 Trying
  inject_msg(respond_to_txdata(invite_txdata, 100), &tpAS);

  // ---------- AS1 turns INVITE around
  // (acting as routing B2BUA by adding a Via, and removing the top Route.)
  const pj_str_t STR_VIA = pj_str("Via");
  pjsip_via_hdr* via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(invite_txdata->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK2222222222");
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for 6505551234)
  SCOPED_TRACE("INVITE (S)");
  invite_txdata = pop_txdata();
  out = invite_txdata->msg;
  r1 = ReqMatcher("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpAS.expect_target(invite_txdata, false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=TCP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=TCP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;sescase=term;regstate=reg"));

  // AS1 sends an immediate 100 Trying
  inject_msg(respond_to_txdata(invite_txdata, 100), &tpAS);

  // ---------- AS1 turns INVITE around
  // (acting as routing B2BUA by adding a Via, and removing the top Route.)
  via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(invite_txdata->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK3333333333"); // Must differ from previous branch
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed to terminating UE
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // Save the request for later.
  pjsip_tx_data* target_rq = pop_txdata();

  // Bono sends an immediate 100 Trying response.
  inject_msg(respond_to_txdata(target_rq, 100), &tpBono);

  // The terminating UE doesn't respond so eventually the transaction will time
  // out.  To force this to happen in the right way, we send a CANCEL chasing
  // the original transaction (which is what Bono will do if the transaction
  // times out).
  msg._method = "CANCEL";
  msg._via = "10.99.88.11:12345";
  msg._branch = "1111111111";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";
  inject_msg(msg.get_request(), &tpBono);

  // CANCEL gets OK'd
  ASSERT_EQ(2, txdata_count());
  RespMatcher(200).matches(current_txdata()->msg);
  free_txdata();

  // The CANCEL is forwarded to AS1 (as originating AS)
  ReqMatcher("CANCEL").matches(current_txdata()->msg);

  // AS1 responds to the CANCEL.
  inject_msg(respond_to_current_txdata(200), &tpAS);
  free_txdata();

  // AS1 forwards the CANCEL back to Sprout.
  msg._branch = "2222222222";
  inject_msg(msg.get_request(), &tpAS);

  // CANCEL gets OK'd
  ASSERT_EQ(2, txdata_count());
  RespMatcher(200).matches(current_txdata()->msg);
  free_txdata();

  // The CANCEL is forwarded to AS1 (as terminating AS)
  ReqMatcher("CANCEL").matches(current_txdata()->msg);

  // AS2 responds to the CANCEL.
  inject_msg(respond_to_current_txdata(200), &tpAS);
  free_txdata();

  // AS1 forwards the CANCEL back to Sprout.
  msg._branch = "3333333333";
  inject_msg(msg.get_request(), &tpAS);

  // CANCEL gets OK'd
  ASSERT_EQ(2, txdata_count());
  RespMatcher(200).matches(current_txdata()->msg);
  free_txdata();

  // The CANCEL is forwarded to the terminating UE
  ReqMatcher("CANCEL").matches(current_txdata()->msg);

  // UE responds to the CANCEL.
  inject_msg(respond_to_current_txdata(200), &tpAS);
  free_txdata();

  // UE sends a 487 response which is ACKed and forwarded to AS1 (as terminating AS)
  inject_msg(respond_to_txdata(target_rq, 487));
  ASSERT_EQ(2, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();
  ASSERT_EQ(1, txdata_count());
  RespMatcher(487).matches(current_txdata()->msg);

  // AS1 ACKs the response and forwards it back to Sprout removing the top Via header.
  msg._method = "ACK";
  msg._branch = "3333333333";
  inject_msg(msg.get_request(), &tpAS);
  out = current_txdata()->msg;
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // Sprout ACKs the response and forwards it to AS1 (as originating AS).
  ASSERT_EQ(2, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();
  ASSERT_EQ(1, txdata_count());
  RespMatcher(487).matches(current_txdata()->msg);

  // AS1 ACKs the response and forwards it back to Sprout removing the top Via header.
  msg._method = "ACK";
  msg._branch = "2222222222";
  inject_msg(msg.get_request(), &tpAS);
  out = current_txdata()->msg;
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // Sprout ACKs the response and forwards it back to the originating UE.
  ASSERT_EQ(2, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();
  ASSERT_EQ(1, txdata_count());
  RespMatcher(487).matches(current_txdata()->msg);
  free_txdata();

  // UE ACKs the response.
  msg._method = "ACK";
  msg._branch = "2222222222";
  inject_msg(msg.get_request(), &tpAS);

  // Session didn't get set up successfully so no session setup time will be
  // tracked.
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}


// Test local MESSAGE request with both originating and terminating ASs where terminating UE doesn't respond.
TEST_F(SCSCFTest, OriginatingTerminatingMessageASTimeout)
{
  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS(TransportFlow::Protocol::TCP, stack_data.scscf_port, "1.2.3.4", 56789);

  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>MESSAGE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=TCP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>MESSAGE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=TCP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  // ---------- Send MESSAGE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._method = "MESSAGE";
  msg._via = "10.99.88.11:12345";
  msg._branch = "1111111111";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";
  inject_msg(msg.get_request(), &tpBono);
  poll();

  // MESSAGE passed on to AS1 (as originating AS for 6505551000)
  ASSERT_EQ(1, txdata_count());
  pjsip_tx_data* message_txdata = pop_txdata();
  pjsip_msg* out = message_txdata->msg;
  ReqMatcher r1("MESSAGE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpAS.expect_target(message_txdata, false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=TCP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=TCP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551000@homedomain>;sescase=orig;regstate=reg"));

  // AS1 sends an immediate 100 Trying response.  This isn't realistic as the
  // response should be delayed by 3.5 seconds, but it stops the script
  // having to handle MESSAGE retransmits.
  inject_msg(respond_to_txdata(message_txdata, 100), &tpAS);

  // Advance time by a second so we have good enough control over the order
  // the transactions time out.
  cwtest_advance_time_ms(1000L);

  // ---------- AS1 turns MESSAGE around
  // (acting as routing B2BUA by adding a Via, and removing the top Route.)
  const pj_str_t STR_VIA = pj_str("Via");
  pjsip_via_hdr* via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(message_txdata->pool);
  via_hdr->transport = pj_str("TCP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK2222222222");
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  pjsip_tx_data_dec_ref(message_txdata);

  // MESSAGE passed on to AS1 (as terminating AS for 6505551234)
  ASSERT_EQ(1, txdata_count());
  message_txdata = pop_txdata();
  out = message_txdata->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpAS.expect_target(message_txdata, false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=TCP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=TCP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;sescase=term;regstate=reg"));

  // AS1 sends an immediate 100 Trying response.  This isn't realistic as the
  // response should be delayed by 3.5 seconds, but it stops the script
  // having to handle MESSAGE retransmits.
  inject_msg(respond_to_txdata(message_txdata, 100), &tpAS);

  // Advance time by a second so we have good enough control over the order
  // the transactions time out.
  cwtest_advance_time_ms(1000L);

  // ---------- AS1 turns MESSAGE around
  // (acting as routing B2BUA by adding a Via, and removing the top Route.)
  via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(message_txdata->pool);
  via_hdr->transport = pj_str("TCP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK3333333333"); // Must differ from previous branch
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  pjsip_tx_data_dec_ref(message_txdata);

  // MESSAGE passed to terminating UE
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // UE sends an immediate 100 Trying response.  This isn't realistic as the
  // response should be delayed by 3.5 seconds, but it stops the script
  // having to handle MESSAGE retransmits.
  inject_msg(respond_to_current_txdata(100), &tpBono);

  // Advance the time so the delayed 100 Trying responses are sent by Sprout
  // (should happen 3.5 seconds after the MESSAGE was first received, so we'll
  // advance to just over that time).
  cwtest_advance_time_ms(3500L);
  poll();
  ASSERT_EQ(3, txdata_count());
  RespMatcher(100).matches(current_txdata()->msg);
  tpBono.expect_target(current_txdata(), true);
  free_txdata();
  ASSERT_EQ(2, txdata_count());
  RespMatcher(100).matches(current_txdata()->msg);
  tpAS.expect_target(current_txdata(), true);
  free_txdata();
  ASSERT_EQ(1, txdata_count());
  RespMatcher(100).matches(current_txdata()->msg);
  tpAS.expect_target(current_txdata(), true);
  free_txdata();

  // Now advance the time so the first transaction times out.  This should
  // happen 64*T1=32 seconds after the initial request.  Since we've already
  // advanced time by just over 5.5 seconds, we just need to advance by
  // another 26.5 seconds.
  cwtest_advance_time_ms(26500L);
  poll();

  // Sprout should send a 408 response on the original transaction.
  ASSERT_EQ(1, txdata_count());
  RespMatcher(408).matches(current_txdata()->msg);
  tpBono.expect_target(current_txdata(), true);
  free_txdata();

  // Advance the time by another second so the second hop transaction times out.
  cwtest_advance_time_ms(1000L);
  poll();

  // Sprout should send a 408 response to AS1.
  ASSERT_EQ(1, txdata_count());
  RespMatcher(408).matches(current_txdata()->msg);
  tpAS.expect_target(current_txdata(), true);
  free_txdata();

  // Advance the time by another second so the third hop transaction times out.
  cwtest_advance_time_ms(1000L);
  poll();

  // Sprout should send a 408 response to AS1.
  ASSERT_EQ(1, txdata_count());
  RespMatcher(408).matches(current_txdata()->msg);
  tpAS.expect_target(current_txdata(), true);
  free_txdata();
}


// Test terminating call-diversion AS flow to external URI, with orig-cdiv enabled too.
TEST_F(SCSCFTest, TerminatingDiversionExternalOrigCdiv)
{
  TransportFlow tpBono(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpExternal(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.9.8.7", 5060);

  register_uri(_sdm, _hss_connection, "6505501234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505501234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505501234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_result("/impu/sip%3A6505501234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345";
  msg._to = "6505501234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505501234@homedomain";
  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505501234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505501234@homedomain>;sescase=term;regstate=reg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS);

  // ---------- AS1 turns it around
  // (acting as routing B2BUA by adding a Via, removing the top Route and changing the target)
  const pj_str_t STR_VIA = pj_str("Via");
  pjsip_via_hdr* via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(current_txdata()->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK1234567890");
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  ((pjsip_sip_uri*)out->line.req.uri)->host = pj_str("ut2.cw-ngv.com");
  inject_msg(out, &tpAS);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as originating-cdiv AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  r1 = ReqMatcher("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505501234@ut2.cw-ngv.com", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:1\\.2\\.3\\.4:56789;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;orig;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505501234@homedomain>;orig-cdiv"));

  // ---------- AS2 sends a 100 Trying to indicate it has received the request.
  string fresp2 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp2, &tpAS);

  // ---------- AS1 turns it around
  // (acting as routing B2BUA by adding a Via, removing the top Route and changing the target)
  via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (via_hdr)
  {
    via_hdr->rport_param = via_hdr->sent_by.port;
  }
  via_hdr = pjsip_via_hdr_create(current_txdata()->pool);
  via_hdr->transport = pj_str("FAKE_UDP");
  via_hdr->sent_by.host = pj_str("1.2.3.4");
  via_hdr->sent_by.port = 56789;
  via_hdr->rport_param = 0;
  via_hdr->branch_param = pj_str("z9hG4bK1234567891"); // Must differ from previous branch
  pjsip_msg_insert_first_hdr(out, (pjsip_hdr*)via_hdr);
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  ((pjsip_sip_uri*)out->line.req.uri)->host = pj_str("ut.cw-ngv.com");
  inject_msg(out, &tpAS);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed externally
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpExternal.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505501234@ut.cw-ngv.com", r1.uri());
  EXPECT_EQ("", get_headers(out, "Route"));

  // ---------- Externally accepted with 200.
  string fresp = respond_to_txdata(current_txdata(), 200);
  free_txdata();
  inject_msg(fresp, &tpExternal);

  // 200 OK goes back to AS1 (orig-cdiv)
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);

  // ---------- AS1 forwards 200 (stripping via)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 200 OK goes back to AS1 (terminating)
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpAS.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);

  // ---------- AS1 forwards 200 (stripping via)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_VIA, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS);
  free_txdata();

  // 200 OK goes back to bono
  out = current_txdata()->msg;
  RespMatcher(200).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  //  We should have tracked the session setup time for just the original session.
  EXPECT_EQ(1, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_audio_session_setup_time_tbl)->_count);
  EXPECT_EQ(0, ((SNMP::FakeEventAccumulatorTable*)_scscf_sproutlet->_video_session_setup_time_tbl)->_count);
}

// This tests that a INVITE with a P-Profile-Key header sends
// a request to Homestead with the correct wildcard entry
TEST_F(SCSCFTest, TestInvitePProfileKey)
{
  SCOPED_TRACE("");
  std::string wildcard = "sip:650![0-9]+!@homedomain";

  // This UT is unrealistic as we're using the same P-Profile-Key header for
  // both the originating and the terminating side; this is OK though for what
  // we're testing
  _hss_connection->set_impu_result("sip:6515551000@homedomain",
                                   "call",
                                   RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6515551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6515551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>",
                                   "",
                                   wildcard);
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>",
                                   "",
                                   wildcard);
  _hss_connection->set_result("/impu/sip%3A6515551000%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  register_uri(_sdm, _hss_connection, "6515551000", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Profile-Key: <" + PJUtils::escape_string_for_uri(wildcard) + ">";
  msg._to = "6515551000";
  msg._requri = "sip:6515551000@homedomain";
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestAddSecondTelPAIHdr)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Asserted-Identity: Andy <sip:6505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain>", "P-Asserted-Identity: \"Andy\" <tel:6505551000>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

// Checks that a tel URI alias is added to the P-Asserted-Identity header even
// when the username is different from the sip URI.
TEST_F(SCSCFTest, TestAddSecondTelPAIHdrWithAlias)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551001</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Asserted-Identity: Andy <sip:6505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain>", "P-Asserted-Identity: \"Andy\" <tel:6505551001>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

// Checks if we have multiple aliases and none of them matches the SIP URI
// supplied that we add the first tel URI on the alias list to the
// P-Asserted-Identity header.
TEST_F(SCSCFTest, TestAddSecondTelPAIHdrMultipleAliasesNoMatch)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551003</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551002</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Asserted-Identity: Andy <sip:6505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain>", "P-Asserted-Identity: \"Andy\" <tel:6505551003>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

// Checks if we have multiple aliases and one of them matches the SIP URI
// supplied that we add the matching alias even if it's not the first on the
// alias list.
TEST_F(SCSCFTest, TestAddSecondTelPAIHdrMultipleAliases)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551003</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Asserted-Identity: Andy <sip:6505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain>", "P-Asserted-Identity: \"Andy\" <tel:6505551000>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}
TEST_F(SCSCFTest, TestAddSecondSIPPAIHdr)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("tel:6505551000", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Asserted-Identity: Andy <tel:6505551000>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <tel:6505551000>", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain;user=phone>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

// Checks that a matching SIP URI is added to the P-Asserted-Identity header
// even when there is no alias of the original tel URI.
TEST_F(SCSCFTest, TestAddSecondSIPPAIHdrNoSIPUri)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("tel:6505551000", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Asserted-Identity: Andy <tel:6505551000>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <tel:6505551000>", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain;user=phone>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestTwoPAIHdrsAlready)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._extra = "P-Asserted-Identity: Andy <sip:6505551000@homedomain>\nP-Asserted-Identity: Andy <tel:6505551111>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain>", "P-Asserted-Identity: \"Andy\" <tel:6505551111>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestNoPAIHdrs)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestPAIHdrODIToken)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:odi_dgds89gd8gdshds@127.0.0.1;orig>";
  msg._extra = "P-Asserted-Identity: Andy <sip:6505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

TEST_F(SCSCFTest, TestNoSecondPAIHdrTerm)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "<PublicIdentity><Identity>tel:6505551000</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  Message msg;
  msg._extra = "P-Asserted-Identity: Andy <sip:6505551000@homedomain>";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("P-Asserted-Identity", "P-Asserted-Identity: \"Andy\" <sip:6505551000@homedomain>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false);
}

/// Test handling of 430 Flow Failed response
TEST_F(SCSCFTest, FlowFailedResponse)
{
  TransportFlow tpBono(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.99.88.11", 12345);
  //TransportFlow tpExternal(TransportFlow::Protocol::UDP, stack_data.scscf_port, "10.9.8.7", 5060);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  std::string user = "sip:6505550231@homedomain";
  register_uri(_sdm, _hss_connection, "6505550231", "homedomain", "sip:f5cc3de4334589d89c661a7acf228ed7@10.114.61.213", 30);

  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");
  _hss_connection->set_impu_result("sip:6505550231@homedomain", "dereg-timeout", RegDataXMLUtils::STATE_REGISTERED,
                              "<IMSSubscription><ServiceProfile>\n"
                              "  <PublicIdentity><Identity>sip:6505550231@homedomain</Identity></PublicIdentity>\n"
                              "  <InitialFilterCriteria>\n"
                              "    <Priority>1</Priority>\n"
                              "    <TriggerPoint>\n"
                              "      <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                              "      <SPT>\n"
                              "        <ConditionNegated>0</ConditionNegated>\n"
                              "        <Group>0</Group>\n"
                              "        <Method>REGISTER</Method>\n"
                              "        <Extension></Extension>\n"
                              "      </SPT>\n"
                              "    </TriggerPoint>\n"
                              "    <ApplicationServer>\n"
                              "      <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                              "      <DefaultHandling>1</DefaultHandling>\n"
                              "    </ApplicationServer>\n"
                              "  </InitialFilterCriteria>\n"
                              "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505550231%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345";
  msg._to = "65055502314@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505550231@homedomain";
  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);
  msg.set_route(out);
  free_txdata();

  // INVITE passed externally
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("INVITE").matches(out));

  // Send 430 Flow Failed response.
  string fresp = respond_to_current_txdata(430);
  free_txdata();
  inject_msg(fresp);

  // Sprout ACKs the response.
  ASSERT_EQ(3, txdata_count());
  ReqMatcher("ACK").matches(current_txdata()->msg);
  free_txdata();

  // Sprout deletes the binding.
  SubscriberDataManager::AoRPair* aor_data = _sdm->get_aor_data(user, 0);
  ASSERT_TRUE(aor_data != NULL);
  EXPECT_EQ(0u, aor_data->get_current()->_bindings.size());
  delete aor_data; aor_data = NULL;

  // Because there are no remaining bindings, Sprout sends a deregister to the
  // HSS and a third-party deREGISTER to the AS.
  ASSERT_EQ(2, txdata_count());
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(ReqMatcher("REGISTER").matches(out));
  EXPECT_EQ(NULL, out->body);

  // Send a 200 OK response from the AS.
  fresp = respond_to_current_txdata(200);
  //free_txdata();
  inject_msg(fresp, &tpAS);

  // Catch the forwarded 430 response.
  ASSERT_EQ(1, txdata_count());
  out = current_txdata()->msg;
  RespMatcher(430).matches(out);
  free_txdata();

  // UE ACKs the response.
  msg._method = "ACK";
  inject_msg(msg.get_request(), &tpBono);
}

// Check that if an AS supplies a preloaded route when routing back to the
// S-CSCF, we follow the route and record route ourselves. This is needed for
// routing to non-registering PBXs, where the AS preloads the path to the PBX.

// Check that sprout follows a preloaded route when the AS has changed the
// request URI.
TEST_F(SCSCFTest, PreloadedRouteChangedReqUri)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                      <ConditionTypeCNF>0</ConditionTypeCNF>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <Method>INVITE</Method>
                                        <Extension></Extension>
                                      </SPT>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                        <Extension></Extension>
                                      </SPT>
                                    </TriggerPoint>
                                    <ApplicationServer>
                                      <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                      <DefaultHandling>0</DefaultHandling>
                                    </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // ---------- AS1 sends the request back top the S-CSCF. It changes the
  // request URI and pre-loads a route.
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }

  char preloaded_route[80] = "sip:3.3.3.3:5060;transport=TCP;lr";
  pjsip_route_hdr* hroute = pjsip_route_hdr_create(current_txdata()->pool);
  hroute->name_addr.uri =
    (pjsip_uri*)pjsip_parse_uri(current_txdata()->pool,
                                preloaded_route,
                                strlen(preloaded_route),
                                0);
  pjsip_msg_add_hdr(out, (pjsip_hdr*)hroute);

  ((pjsip_sip_uri*)out->line.req.uri)->user = pj_str("newtarget");
  ((pjsip_sip_uri*)out->line.req.uri)->host = pj_str("2.2.2.2");

  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  // Sprout has preserved the target and route.
  EXPECT_EQ("sip:newtarget@2.2.2.2", r1.uri());
  EXPECT_EQ(get_headers(out, "Route"),
            "Route: <sip:3.3.3.3:5060;transport=TCP;lr>");
  // Sprout has also record-routed itself.
  EXPECT_THAT(get_headers(out, "Record-Route"),
              MatchesRegex("Record-Route: <sip:scscf.sprout.homedomain:5058;.*billing-role=charge-term.*>"));

  EXPECT_EQ(1, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_routed_by_preloaded_route_tbl)->_count);
  free_txdata();
}


// Check that sprout follows a preloaded route when the AS has NOT changed the
// request URI.
TEST_F(SCSCFTest, PreloadedRoutePreserveReqUri)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                      <ConditionTypeCNF>0</ConditionTypeCNF>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <Method>INVITE</Method>
                                        <Extension></Extension>
                                      </SPT>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                        <Extension></Extension>
                                      </SPT>
                                    </TriggerPoint>
                                    <ApplicationServer>
                                      <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                      <DefaultHandling>0</DefaultHandling>
                                    </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());

  // ---------- AS1 sends the request back top the S-CSCF. It preserves the
  // request URI but pre-loads a route.
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }

  char preloaded_route[80] = "sip:3.3.3.3:5060;transport=TCP;lr";
  pjsip_route_hdr* hroute = pjsip_route_hdr_create(current_txdata()->pool);
  hroute->name_addr.uri =
    (pjsip_uri*)pjsip_parse_uri(current_txdata()->pool,
                                preloaded_route,
                                strlen(preloaded_route),
                                0);
  pjsip_msg_add_hdr(out, (pjsip_hdr*)hroute);

  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  // Sprout has preserved the target and route.
  EXPECT_EQ(get_headers(out, "Route"),
            "Route: <sip:3.3.3.3:5060;transport=TCP;lr>");
  // Sprout has also record-routed itself.
  EXPECT_THAT(get_headers(out, "Record-Route"),
              MatchesRegex("Record-Route: <sip:scscf.sprout.homedomain:5058;.*billing-role=charge-term.*>"));

  EXPECT_EQ(1, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_routed_by_preloaded_route_tbl)->_count);
  free_txdata();
}


// Check that sprout follows a preloaded route even when there are more ASs in
// the chain.
TEST_F(SCSCFTest, PreloadedRouteNotLastAs)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                      <ConditionTypeCNF>0</ConditionTypeCNF>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <Method>INVITE</Method>
                                        <Extension></Extension>
                                      </SPT>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                        <Extension></Extension>
                                      </SPT>
                                    </TriggerPoint>
                                    <ApplicationServer>
                                      <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                      <DefaultHandling>0</DefaultHandling>
                                    </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                      <ConditionTypeCNF>0</ConditionTypeCNF>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <Method>INVITE</Method>
                                        <Extension></Extension>
                                      </SPT>
                                      <SPT>
                                        <ConditionNegated>0</ConditionNegated>
                                        <Group>0</Group>
                                        <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                        <Extension></Extension>
                                      </SPT>
                                    </TriggerPoint>
                                    <ApplicationServer>
                                      <ServerName>sip:1.2.3.4:56787;transport=UDP</ServerName>
                                      <DefaultHandling>0</DefaultHandling>
                                    </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // ---------- AS1 sends the request back top the S-CSCF. It changes the
  // request URI and pre-loads a route.
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }

  char preloaded_route[80] = "sip:3.3.3.3:5060;transport=TCP;lr";
  pjsip_route_hdr* hroute = pjsip_route_hdr_create(current_txdata()->pool);
  hroute->name_addr.uri =
    (pjsip_uri*)pjsip_parse_uri(current_txdata()->pool,
                                preloaded_route,
                                strlen(preloaded_route),
                                0);
  pjsip_msg_add_hdr(out, (pjsip_hdr*)hroute);

  // Re-target the request to a new user. Use the domain "newdomain" as this
  // will be routed off net by the BGCF.
  ((pjsip_sip_uri*)out->line.req.uri)->user = pj_str("newtarget");
  ((pjsip_sip_uri*)out->line.req.uri)->host = pj_str("2.2.2.2");
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  SCOPED_TRACE("INVITE (4)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpBono.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:newtarget@2.2.2.2", r1.uri());
  // Sprout has preserved the target and route.
  EXPECT_EQ(get_headers(out, "Route"),
            "Route: <sip:3.3.3.3:5060;transport=TCP;lr>");
  // Sprout has also record-routed itself.
  EXPECT_THAT(get_headers(out, "Record-Route"),
              MatchesRegex("Record-Route: <sip:scscf.sprout.homedomain:5058;.*billing-role=charge-term.*>"));

  EXPECT_EQ(1, ((SNMP::FakeCounterTable*)_scscf_sproutlet->_routed_by_preloaded_route_tbl)->_count);
  free_txdata();
}

TEST_F(SCSCFTest, AutomaticRegistration)
{
  SCOPED_TRACE("");

  // Create an originating request that has a proxy-authorization header and
  // requires automatic registration.
  Message msg;
  msg._to = "newuser";
  msg._todomain = "domainvalid";
  msg._route = "Route: <sip:sprout.homedomain;orig;auto-reg>";
  msg._extra = "Proxy-Authorization: Digest username=\"kermit\", realm=\"homedomain\", uri=\"sip:6505551000@homedomain\", algorithm=MD5";

  // The HSS expects to be invoked with a request type of "reg" and with the
  // right private ID.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "reg", RegDataXMLUtils::STATE_REGISTERED, "", "?private_id=kermit");

  add_host_mapping("domainvalid", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route", "Route: <sip:10.0.0.1:5060;transport=TCP;lr>"));
  doSuccessfulFlow(msg, testing::MatchesRegex("sip:newuser@domainvalid"), hdrs);
}

TEST_F(SCSCFTest, AutomaticRegistrationDerivedIMPI)
{
  SCOPED_TRACE("");

  // Create an originating request that requires automatic registration.
  Message msg;
  msg._to = "newuser";
  msg._todomain = "domainvalid";
  msg._route = "Route: <sip:sprout.homedomain;orig;auto-reg>";

  // The HSS expects to be invoked with a request type of "reg". No
  // Proxy-Authorization present, so derive the IMPI from the IMPU.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "reg", RegDataXMLUtils::STATE_REGISTERED, "", "?private_id=6505551000%40homedomain");

  add_host_mapping("domainvalid", "10.9.8.7");
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route", "Route: <sip:10.0.0.1:5060;transport=TCP;lr>"));
  doSuccessfulFlow(msg, testing::MatchesRegex("sip:newuser@domainvalid"), hdrs);
}

TEST_F(SCSCFTest, TestSessionExpires)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  // Send an INVITE where the client supports session timers. This means that
  // if the server does not support timers, there should still be a
  // Session-Expires header on the response.
  //
  // Most of the session timer logic is tested in
  // `session_expires_helper_test.cpp`. This is just to check that the S-CSCF
  // invokes the logic correctly.
  Message msg;
  msg._extra = "Session-Expires: 600\r\nSupported: timer";
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Session-Expires", "Session-Expires:.*"));
  list<HeaderMatcher> rsp_hdrs;
  rsp_hdrs.push_back(HeaderMatcher("Session-Expires", "Session-Expires: .*"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs, false, rsp_hdrs);
}

TEST_F(SCSCFTest, TestSessionExpiresInDialog)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED, "");

  // Send an UPDATE in-dialog request to which we should always add RR and SE.
  // Then check that if the UAS strips the SE, that Sprout tells the UAC to be
  // the refresher. This ensures that our response processing is correct.
  Message msg;
  msg._extra = "Supported: timer";
  msg._in_dialog = true;

  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Record-Route"));
  hdrs.push_back(HeaderMatcher("Session-Expires", "Session-Expires:.*"));

  list<HeaderMatcher> rsp_hdrs;
  rsp_hdrs.push_back(HeaderMatcher("Session-Expires", "Session-Expires:.*;refresher=uac"));
  rsp_hdrs.push_back(HeaderMatcher("Record-Route"));

  doSuccessfulFlow(msg, testing::MatchesRegex(".*homedomain.*"), hdrs, false, rsp_hdrs);
}

TEST_F(SCSCFTest, TestSessionExpiresWhenNoRecordRoute)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:4.2.3.4:56788;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>1</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);
  TransportFlow tpAS2(TransportFlow::Protocol::UDP, stack_data.scscf_port, "4.2.3.4", 56788);


  // Send an INVITE
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._requri = "sip:6505551234@homedomain";
  msg._method = "INVITE";

  pjsip_msg* out;
  inject_msg(msg.get_request());

  // INVITE passed to AS1
  SCOPED_TRACE("INVITE (1)");
  ASSERT_EQ(2, txdata_count());
  free_txdata();
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  ASSERT_TRUE(!get_headers(out, "Record-Route").empty());
  ASSERT_TRUE(!get_headers(out, "Session-Expires").empty());


  // AS proxies INVITE back.
  const pj_str_t STR_ROUTE = pj_str("Route");
  const pj_str_t STR_REC_ROUTE = pj_str("Record-Route");
  const pj_str_t STR_SESS_EXP = pj_str("Session-Expires");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  pjsip_hdr* rr_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_REC_ROUTE, NULL);
  pjsip_hdr* se_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_SESS_EXP, NULL);
  pj_list_erase(rr_hdr);
  pj_list_erase(se_hdr);

  if (hdr)
  {
    pj_list_erase(hdr);
  }

  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS2
  SCOPED_TRACE("INVITE (2)");
  out = current_txdata()->msg;
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpAS2.expect_target(current_txdata(), false);

  // Should not RR between AS's and therefore shouldn't SE
  ASSERT_TRUE(get_headers(out, "Record-Route").empty());
  ASSERT_TRUE(get_headers(out, "Session-Expires").empty());
}


// Test that getting a 503 error from homestead when looking up iFCs results in
// sprout sending a 504 error.
TEST_F(SCSCFTest, HSSTimeoutOnPutRegData)
{
  // Send originating INVITE
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";

  // HSS will return a 503
  _hss_connection->set_rc("/impu/sip%3A6505551000%40homedomain/reg-data", 503);

  inject_msg(msg.get_request());

  // 100 Trying goes out
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // Followed by a 504
  out = current_txdata()->msg;
  EXPECT_EQ(504, out->line.status.code);
  EXPECT_EQ("Server Timeout", str_pj(out->line.status.reason));

  _hss_connection->delete_rc("/impu/sip%3A6505551000%40homedomain/reg-data");
}


// Test that a failure to get iFCs due to a 503 error from homestead during Call
// Diversion results in sprout sending a 504
TEST_F(SCSCFTest, HSSTimeoutOnCdiv)
{
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", RegDataXMLUtils::STATE_REGISTERED,
                                R"(<IMSSubscription><ServiceProfile>
                                <PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>
                                  <InitialFilterCriteria>
                                    <Priority>2</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>4</SessionCase>  <!-- originating-cdiv -->
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                  <InitialFilterCriteria>
                                    <Priority>0</Priority>
                                    <TriggerPoint>
                                    <ConditionTypeCNF>0</ConditionTypeCNF>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <Method>INVITE</Method>
                                      <Extension></Extension>
                                    </SPT>
                                    <SPT>
                                      <ConditionNegated>0</ConditionNegated>
                                      <Group>0</Group>
                                      <SessionCase>1</SessionCase>  <!-- terminating-registered -->
                                      <Extension></Extension>
                                    </SPT>
                                  </TriggerPoint>
                                  <ApplicationServer>
                                    <ServerName>sip:5.2.3.4:56787;transport=UDP</ServerName>
                                    <DefaultHandling>0</DefaultHandling>
                                  </ApplicationServer>
                                  </InitialFilterCriteria>
                                </ServiceProfile></IMSSubscription>)");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "5.2.3.4", 56787);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._via = "10.99.88.11:12345;transport=TCP";
  msg._to = "6505551234@homedomain";
  msg._todomain = "";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpBono.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1 (as terminating AS for Bob)
  SCOPED_TRACE("INVITE (S)");
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);
  EXPECT_EQ("sip:6505551234@homedomain", r1.uri());
  EXPECT_THAT(get_headers(out, "Route"),
              testing::MatchesRegex("Route: <sip:5\\.2\\.3\\.4:56787;transport=UDP;lr>\r\nRoute: <sip:odi_[+/A-Za-z0-9]+@127.0.0.1:5058;transport=UDP;lr;service=scscf>"));
  EXPECT_THAT(get_headers(out, "P-Served-User"),
              testing::MatchesRegex("P-Served-User: <sip:6505551234@homedomain>;sescase=term;regstate=reg"));

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp1 = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp1, &tpAS1);

  // The next request to the HSS will get a 503 response
  _hss_connection->delete_result("sip:6505551234@homedomain");
  _hss_connection->set_rc("/impu/sip%3A6505551234%40homedomain/reg-data", 503);

  // ---------- AS1 turns it around (acting as routing B2BUA by changing the target)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  ((pjsip_sip_uri*)out->line.req.uri)->user = pj_str("6505555678");
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  msg.set_route(out);
  free_txdata();

  // Followed by a 504 (since the iFC lookup has got a 503)
  out = current_txdata()->msg;
  RespMatcher(504).matches(out);
  tpAS1.expect_target(current_txdata(), true);  // Requests always come back on same transport
  free_txdata();

  _hss_connection->delete_rc("/impu/sip%3A6505551000%40homedomain/reg-data");
}

TEST_F(SCSCFTest, TestAddStoredPathHeader)
{
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  SCOPED_TRACE("");

  // Add a binding with the _path_headers and _path_uris set.
  string uri("sip:6505551234@homedomain");
  string contact("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result(uri, "call", RegDataXMLUtils::STATE_REGISTERED, "");
  SubscriberDataManager::AoRPair* aor = _sdm->get_aor_data(uri, 0);
  SubscriberDataManager::AoR::Binding* binding = aor->get_current()->get_binding(contact);
  binding->_uri = contact;
  binding->_cid = "1";
  binding->_cseq = 1;
  binding->_path_uris.push_back(std::string("sip:abcdefgh@ut.cw-ngv.com;lr"));
  binding->_path_headers.push_back(std::string("\"Bob\" <sip:abcdefgh@ut.cw-ngv.com;lr>;tag=6ht7"));
  binding->_expires = time(NULL) + 300;
  binding->_priority = 1000;
  binding->_emergency_registration = false;
  AssociatedURIs associated_uris = {};
  associated_uris.add_uri(uri, false);
  bool ret = _sdm->set_aor_data(uri, &associated_uris, aor, 0);
  delete aor;
  EXPECT_TRUE(ret);

  // Check that the Route header contains the full path header form the binding.
  Message msg;
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route", "Route: \"Bob\" <sip:abcdefgh@ut.cw-ngv.com;lr>;tag=6ht7"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);
}

TEST_F(SCSCFTest, TestAddStoredPathURI)
{
  add_host_mapping("ut.cw-ngv.com", "10.9.8.7");
  SCOPED_TRACE("");

  // Add a binding with only _path_uris set.
  string uri("sip:6505551234@homedomain");
  string contact("sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result(uri, "call", RegDataXMLUtils::STATE_REGISTERED, "");
  SubscriberDataManager::AoRPair* aor = _sdm->get_aor_data(uri, 0);
  SubscriberDataManager::AoR::Binding* binding = aor->get_current()->get_binding(contact);
  binding->_uri = contact;
  binding->_cid = "1";
  binding->_cseq = 1;
  binding->_path_uris.push_back(std::string("sip:abcdefgh@ut.cw-ngv.com;lr"));
  binding->_expires = time(NULL) + 300;
  binding->_priority = 1000;
  binding->_emergency_registration = false;
  AssociatedURIs associated_uris = {};
  associated_uris.add_uri(uri, false);
  bool ret = _sdm->set_aor_data(uri, &associated_uris, aor, 0);
  delete aor;
  EXPECT_TRUE(ret);

  // Check that the Route header contains the URI part of the path header.
  Message msg;
  list<HeaderMatcher> hdrs;
  hdrs.push_back(HeaderMatcher("Route", "Route: <sip:abcdefgh@ut.cw-ngv.com;lr>"));
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);
}

TEST_F(SCSCFTest, TestCallerNotBarred)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // The primary IMPU is barred, but this shouldn't stop us making a call since
  // we are calling from one of the other IMPUs.
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551001@homedomain</Identity><BarringIndication>1</BarringIndication></PublicIdentity>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>1</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  Message msg;
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);
}

TEST_F(SCSCFTest, TestCalleeNotBarred)
{
  SCOPED_TRACE("");

  // Need to use the first unbarred identity since that is the key used in memcached.
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");

  // The primary IMPU is barred, but this shouldn't stop us making a call since
  // we are calling one of the other IMPUs.
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551235@homedomain</Identity><BarringIndication>1</BarringIndication></PublicIdentity>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>1</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  Message msg;
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);
}

// Test emergency registrations receive calls when barred.
TEST_F(SCSCFTest, TestEmergencyCalleeNotBarred)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;sos;ob", 3600, "", true);
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity><BarringIndication>1</BarringIndication></PublicIdentity>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>1</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  Message msg;
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);
}

// Test only emergency registrations in an implicit registration set receive
// calls to barred IMPUs.
TEST_F(SCSCFTest, TestEmergencyMultipleBindings)
{
  SCOPED_TRACE("");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;sos;ob", 3600, "", true);
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:fowertreetoowun@10.114.61.213:5061;transport=tcp;ob", 3600, "", false);
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", "REGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity><BarringIndication>1</BarringIndication></PublicIdentity>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>1</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  Message msg;
  list<HeaderMatcher> hdrs;
  doSuccessfulFlow(msg, testing::MatchesRegex(".*wuntootreefower.*"), hdrs);
}

// Check that a request with no matching iFCs is rejected on originating side.
TEST_F(SCSCFTest, NoMatchingiFCsRejectOrig)
{
  _scscf_sproutlet->_ifc_configuration._reject_if_no_matching_ifcs = true;
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "    <Priority>0</Priority>\n"
                                   "    <TriggerPoint>\n"
                                   "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                   "    <SPT>\n"
                                   "      <ConditionNegated>0</ConditionNegated>\n"
                                   "      <Group>0</Group>\n"
                                   "      <Method>PUBLISH</Method>\n"
                                   "      <Extension></Extension>\n"
                                   "    </SPT>\n"
                                   "  </TriggerPoint>\n"
                                   "  <ApplicationServer>\n"
                                   "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                   "    <DefaultHandling>0</DefaultHandling>\n"
                                   "  </ApplicationServer>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // Request is rejected with a 400.
  out = current_txdata()->msg;
  RespMatcher(400).matches(out);
  tpBono.expect_target(current_txdata(), true);
  free_txdata();
}

// Check that a request with no matching iFCs is rejected on terminating side.
TEST_F(SCSCFTest, NoMatchingiFCsRejectTerminating)
{
  _scscf_sproutlet->_ifc_configuration._reject_if_no_matching_ifcs = true;
  _hss_connection->set_impu_result("sip:6505551234@homedomain", "call", "UNREGISTERED",
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551234@homedomain</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "    <Priority>0</Priority>\n"
                                   "    <TriggerPoint>\n"
                                   "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                   "    <SPT>\n"
                                   "      <ConditionNegated>0</ConditionNegated>\n"
                                   "      <Group>0</Group>\n"
                                   "      <Method>PUBLISH</Method>\n"
                                   "      <Extension></Extension>\n"
                                   "    </SPT>\n"
                                   "  </TriggerPoint>\n"
                                   "  <ApplicationServer>\n"
                                   "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                   "    <DefaultHandling>0</DefaultHandling>\n"
                                   "  </ApplicationServer>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // Request is rejected with a 400.
  out = current_txdata()->msg;
  RespMatcher(400).matches(out);
  tpBono.expect_target(current_txdata(), true);
  free_txdata();
}

// Test that we use fallback iFCs if there are no matching iFCs, and that the
// application server flows are as expected.
TEST_F(SCSCFTest, NoMatchingStandardiFCsUseFallbackiFCs)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _scscf_sproutlet->_ifc_configuration._apply_fallback_ifcs = true;
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "    <Priority>0</Priority>\n"
                                   "    <TriggerPoint>\n"
                                   "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                   "    <SPT>\n"
                                   "      <ConditionNegated>0</ConditionNegated>\n"
                                   "      <Group>0</Group>\n"
                                   "      <Method>PUBLISH</Method>\n"
                                   "      <Extension></Extension>\n"
                                   "    </SPT>\n"
                                   "    </TriggerPoint>\n"
                                   "    <ApplicationServer>\n"
                                   "      <ServerName>sip:1.2.3.5:56789;transport=UDP</ServerName>\n"
                                   "      <DefaultHandling>0</DefaultHandling>\n"
                                   "    </ApplicationServer>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.5", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // INVITE passed on to AS1
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  out = current_txdata()->msg;
  ReqMatcher r3("INVITE");
  ASSERT_NO_FATAL_FAILURE(r3.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);
  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}

// Test that we use fallback iFCs if there are no matching iFCs, and that the
// application server flows are as expected. In this case we don't have any
// standard iFCs at all.
TEST_F(SCSCFTest, NoStandardiFCsUseFallbackiFCs)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _scscf_sproutlet->_ifc_configuration._apply_fallback_ifcs = true;
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.5", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // INVITE passed on to AS1
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  out = current_txdata()->msg;
  ReqMatcher r3("INVITE");
  ASSERT_NO_FATAL_FAILURE(r3.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);
  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}

// Test that if a user only has dummy application servers, then no application
// servers are triggered.
TEST_F(SCSCFTest, OnlyDummyApplicationServers)
{
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                   "<IMSSubscription><ServiceProfile>\n"
                                   "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                   "  <InitialFilterCriteria>\n"
                                   "    <Priority>0</Priority>\n"
                                   "    <TriggerPoint>\n"
                                   "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                   "    <SPT>\n"
                                   "      <ConditionNegated>0</ConditionNegated>\n"
                                   "      <Group>0</Group>\n"
                                   "      <Method>INVITE</Method>\n"
                                   "      <Extension></Extension>\n"
                                   "    </SPT>\n"
                                   "  </TriggerPoint>\n"
                                   "  <ApplicationServer>\n"
                                   "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                   "    <DefaultHandling>0</DefaultHandling>\n"
                                   "  </ApplicationServer>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "  <InitialFilterCriteria>\n"
                                   "    <Priority>1</Priority>\n"
                                   "    <TriggerPoint>\n"
                                   "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                   "    <SPT>\n"
                                   "      <ConditionNegated>0</ConditionNegated>\n"
                                   "      <Group>0</Group>\n"
                                   "      <Method>INVITE</Method>\n"
                                   "      <Extension></Extension>\n"
                                   "    </SPT>\n"
                                   "  </TriggerPoint>\n"
                                   "  <ApplicationServer>\n"
                                   "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                   "    <DefaultHandling>0</DefaultHandling>\n"
                                   "  </ApplicationServer>\n"
                                   "  </InitialFilterCriteria>\n"
                                   "</ServiceProfile></IMSSubscription>");

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  free_txdata();

  // Check that there's a 404 - no attempt to send via an application server
  // (it's a 404 as the terminating subscriber isn't registered to make this UT
  // simpler).
  out = current_txdata()->msg;
  RespMatcher(404).matches(out);
  tpBono.expect_target(current_txdata(), true);
  free_txdata();
}

// Test that if a user has a mix of real and dummy application servers, only
// the real application servers are triggered.
TEST_F(SCSCFTest, MixedRealAndDummyApplicationServer)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>0</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.2.3.4:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>2</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");
  EXPECT_CALL(*_sess_cont_comm_tracker, on_success(StrEq("sip:1.2.3.4:56789;transport=UDP")));

  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);
  TransportFlow tpAS1(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.2.3.4", 56789);

  // ---------- Send INVITE
  // We're within the trust boundary, so no stripping should occur.
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS1
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));

  tpAS1.expect_target(current_txdata(), false);

  // ---------- AS1 sends a 100 Trying to indicate it has received the request.
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS1);

  // ---------- AS1 turns it around (acting as proxy)
  const pj_str_t STR_ROUTE = pj_str("Route");
  pjsip_hdr* hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  if (hdr)
  {
    pj_list_erase(hdr);
  }
  inject_msg(out, &tpAS1);
  free_txdata();

  // 100 Trying goes back to AS1
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);
  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}


// These MMF tests are essentially the MixedRealAndDummyApplicationServer test,
// but with simulated MMF processing between the SCSCF and the AS
TEST_F(SCSCFTest, MMFPreAs)
{
  TRC_DEBUG("TEst");
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>0</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:pre.as.only.mmf.test.server:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>2</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpMMFpreAS(TransportFlow::Protocol::TCP, stack_data.scscf_port, "11.22.33.44", 5053);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "pre.as.only.mmf.test.server", 56789);
  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // Send the INVITE
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  TRC_DEBUG("inject");
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to MMF
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpMMFpreAS.expect_target(current_txdata(), false);

  // MMF sends a 100 Trying
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpMMFpreAS);

  const pj_str_t STR_ROUTE = pj_str("Route");

  // Ensure the pre-as header was added as expected, and remove it
  pjsip_hdr* preas_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  std::string preas_uri = PJUtils::get_header_value(preas_hdr);
  EXPECT_THAT(preas_uri, MatchesRegex(".*sip:11.22.33.44:5053.*"));
  EXPECT_THAT(preas_uri, MatchesRegex(".*namespace=mmf.*"));
  EXPECT_THAT(preas_uri, MatchesRegex(".*mmfscope=pre-as.*"));
  EXPECT_THAT(preas_uri, MatchesRegex(".*mmftarget=PreASOnly.*"));
  pj_list_erase(preas_hdr);

  // Ensure the AS header was added as expected, and remove it
  pjsip_hdr* as_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  std::string as_uri = PJUtils::get_header_value(as_hdr);
  EXPECT_THAT(as_uri, MatchesRegex(".*pre.as.only.mmf.test.server:56789.*"));
  pj_list_erase(as_hdr);

  // We have removed route headers to simulate the request being routed from
  // the MMF server to the AS.
  // Simulate the request being routed from the AS back to the SCSCF
  inject_msg(out, &tpAS);
  free_txdata();

  // 100 Trying goes back to the AS
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);
  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}


TEST_F(SCSCFTest, MMFPostAs)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>0</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:1.5.8.1:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>2</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpMMFpostAS(TransportFlow::Protocol::TCP, stack_data.scscf_port, "44.33.22.11", 5053);
  TransportFlow tpAS(TransportFlow::Protocol::UDP, stack_data.scscf_port, "1.5.8.1", 56789);
  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // Send the INVITE
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to AS
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpAS.expect_target(current_txdata(), false);

  // AS sends a 100 Trying
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpAS);

  const pj_str_t STR_ROUTE = pj_str("Route");

  // Ensure the AS header was added as expected, and remove it
  pjsip_hdr* as_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  std::string as_uri = PJUtils::get_header_value(as_hdr);
  EXPECT_THAT(as_uri, MatchesRegex(".*1.5.8.1:56789.*"));
  pj_list_erase(as_hdr);

  // Ensure the post-as header was added as expected, and remove it
  pjsip_hdr* postas_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  std::string postas_uri = PJUtils::get_header_value(postas_hdr);
  EXPECT_THAT(postas_uri, MatchesRegex(".*sip:44.33.22.11:5053.*"));
  EXPECT_THAT(postas_uri, MatchesRegex(".*namespace=mmf.*"));
  EXPECT_THAT(postas_uri, MatchesRegex(".*mmfscope=post-as.*"));
  EXPECT_THAT(postas_uri, MatchesRegex(".*mmftarget=PostASOnly.*"));
  pj_list_erase(postas_hdr);

  // We have removed route headers to simulate the request being routed to the
  // AS, and then routed from the AS to our MMF server.
  // Simulate the request being routed from the MMF server back to the SCSCF
  inject_msg(out, &tpMMFpostAS);
  free_txdata();

  // 100 Trying goes back to MMF
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);
  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}


TEST_F(SCSCFTest, MMFPreAndPostAs)
{
  register_uri(_sdm, _hss_connection, "6505551234", "homedomain", "sip:wuntootreefower@10.114.61.213:5061;transport=tcp;ob");
  _hss_connection->set_impu_result("sip:6505551000@homedomain", "call", "UNREGISTERED",
                                "<IMSSubscription><ServiceProfile>\n"
                                "<PublicIdentity><Identity>sip:6505551000@homedomain</Identity></PublicIdentity>"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>0</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>1</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:preandpost.mmf.test.server:56789;transport=UDP</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "  <InitialFilterCriteria>\n"
                                "    <Priority>2</Priority>\n"
                                "    <TriggerPoint>\n"
                                "    <ConditionTypeCNF>0</ConditionTypeCNF>\n"
                                "    <SPT>\n"
                                "      <ConditionNegated>0</ConditionNegated>\n"
                                "      <Group>0</Group>\n"
                                "      <Method>INVITE</Method>\n"
                                "      <Extension></Extension>\n"
                                "    </SPT>\n"
                                "  </TriggerPoint>\n"
                                "  <ApplicationServer>\n"
                                "    <ServerName>sip:DUMMY_AS</ServerName>\n"
                                "    <DefaultHandling>0</DefaultHandling>\n"
                                "  </ApplicationServer>\n"
                                "  </InitialFilterCriteria>\n"
                                "</ServiceProfile></IMSSubscription>");
  _hss_connection->set_result("/impu/sip%3A6505551234%40homedomain/location",
                              "{\"result-code\": 2001,"
                              " \"scscf\": \"sip:scscf.sprout.homedomain:5058;transport=TCP\"}");

  TransportFlow tpMMFpreAS(TransportFlow::Protocol::TCP, stack_data.scscf_port, "11.22.33.44", 5053);
  TransportFlow tpMMFpostAS(TransportFlow::Protocol::TCP, stack_data.scscf_port, "44.33.22.11", 5053);
  TransportFlow tpBono(TransportFlow::Protocol::TCP, stack_data.scscf_port, "10.99.88.11", 12345);

  // Send the INVITE
  Message msg;
  msg._to = "6505551234@homedomain";
  msg._route = "Route: <sip:sprout.homedomain;orig>";
  msg._todomain = "";
  msg._requri = "sip:6505551234@homedomain";

  msg._method = "INVITE";
  inject_msg(msg.get_request(), &tpBono);
  poll();
  ASSERT_EQ(2, txdata_count());

  // 100 Trying goes back to bono
  pjsip_msg* out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to MMF
  out = current_txdata()->msg;
  ReqMatcher r1("INVITE");
  ASSERT_NO_FATAL_FAILURE(r1.matches(out));
  tpMMFpreAS.expect_target(current_txdata(), false);

  // MMF sends a 100 Trying
  string fresp = respond_to_txdata(current_txdata(), 100);
  inject_msg(fresp, &tpMMFpreAS);

  const pj_str_t STR_ROUTE = pj_str("Route");

  // Ensure the pre-as header was added as expected
  pjsip_hdr* preas_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  std::string preas_uri = PJUtils::get_header_value(preas_hdr);
  //
  EXPECT_THAT(preas_uri, MatchesRegex(".*sip:11.22.33.44:5053.*"));
  EXPECT_THAT(preas_uri, MatchesRegex(".*namespace=mmf.*"));
  EXPECT_THAT(preas_uri, MatchesRegex(".*mmfscope=pre-as.*"));
  EXPECT_THAT(preas_uri, MatchesRegex(".*mmftarget=BothPreAndPost.*"));
  pj_list_erase(preas_hdr);

  // Ensure the AS header was added as expected, and remove it
  pjsip_hdr* as_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  std::string as_uri = PJUtils::get_header_value(as_hdr);
  EXPECT_THAT(as_uri, MatchesRegex(".*preandpost.mmf.test.server:56789.*"));
  pj_list_erase(as_hdr);

  // Ensure the post-as header was added as expected, and remove it
  pjsip_hdr* postas_hdr = (pjsip_hdr*)pjsip_msg_find_hdr_by_name(out, &STR_ROUTE, NULL);
  std::string postas_uri = PJUtils::get_header_value(postas_hdr);
  EXPECT_THAT(postas_uri, MatchesRegex(".*sip:44.33.22.11:5053.*"));
  EXPECT_THAT(postas_uri, MatchesRegex(".*namespace=mmf.*"));
  EXPECT_THAT(postas_uri, MatchesRegex(".*mmfscope=post-as.*"));
  EXPECT_THAT(postas_uri, MatchesRegex(".*mmftarget=BothPreAndPost.*"));
  pj_list_erase(postas_hdr);

  // We have removed route headers to simulate the request being routed from
  // the MMF server to the AS, then routed from the AS back to our MMF server.
  // Simulate the request being routed from the MMF server back to the SCSCF
  inject_msg(out, &tpMMFpostAS);
  free_txdata();

  // 100 Trying goes back to MMF
  out = current_txdata()->msg;
  RespMatcher(100).matches(out);
  msg.set_route(out);
  free_txdata();

  // INVITE passed on to final destination
  out = current_txdata()->msg;
  ReqMatcher r2("INVITE");
  ASSERT_NO_FATAL_FAILURE(r2.matches(out));

  tpBono.expect_target(current_txdata(), false);

  // Target sends back 100 Trying
  inject_msg(respond_to_txdata(current_txdata(), 100), &tpBono);
  pjsip_tx_data* txdata = pop_txdata();

  // Send a 200 ringing back down the chain to finish the transaction. This is a
  // more realistic test of AS communication tracking.
  send_response_back_through_dialog(respond_to_txdata(txdata, 200), 200, 2);
  pjsip_tx_data_dec_ref(txdata); txdata = NULL;
}
