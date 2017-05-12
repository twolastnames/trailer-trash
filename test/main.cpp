
#include <queue>
#include <list>
#include <set>

#include <boost/test/included/unit_test.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/foreach.hpp>

#include "RemoteDirectoryUpdater.h"
#include "RemoteTrashEvent.h"
#include "TrashEvent.h"
#include "CanSet.h"

using namespace boost;
using namespace boost::unit_test;
using namespace trailer;

void testRemoteTrashEvent()
{
	using namespace remote;
	using namespace trashevent;
	const int port=23045;
	remote::RemoteTrashEventSettings settings;
	remote::RemoteTrashEvent rte(port,&settings);
	struct : public trashevent::EventListener {
		void add(const string& trashname)
		{
			undefined('a',trashname);
		}
		void schedule(const string& trashname)
		{
			undefined('s',trashname);
		}
		void unschedule(const string& trashname)
		{
			undefined('u',trashname);
		}
		void remove(const string& trashname)
		{
			undefined('r',trashname);
		}
		void undefined(const char event,const string& trashname)
		{
			e=event;
			trash=trashname;
		}
		char e;
		std::string trash;
	} listener;
	rte.addListener(&listener);
	remote::Connection out;
	try {
		rte.writer(boost::asio::ip::host_name(),port,out);
	} catch (const ConnectError& e) {
		BOOST_FAIL(e.errornumber);
	}
	std::string trashname("myTrash");
	out.schedule(trashname);
	this_thread::sleep(posix_time::seconds(2));
	rte.shutdown();
	BOOST_CHECK_EQUAL(listener.trash, trashname);
}

void testRemoteDirectoryUpdater()
{
	using namespace remote;
	using namespace trashevent;
	const int port1=23045;
	const int port2=23046;
	remote::RemoteTrashEventSettings settings;
	remote::RemoteTrashEvent rte1(port1,&settings);
	remote::RemoteTrashEvent rte2(port2,&settings);
	struct : public trashevent::EventListener {
		void add(const string& trashname)
		{
			undefined('a',trashname);
		}
		void schedule(const string& trashname)
		{
			boost::mutex::scoped_lock lock(schedlock);
			if(scheduled.find(trashname)==scheduled.end()) {
				BOOST_FAIL("not here");
			} else {
				scheduled.erase(trashname);
			}
			undefined('s',trashname);
		}
		void unschedule(const string& trashname)
		{
			undefined('u',trashname);
		}
		void remove(const string& trashname)
		{
			undefined('r',trashname);
		}
		void undefined(const char event,const string& trashname)
		{
			e=event;
			trash=trashname;
		}
		char e;
		std::string trash;
		set<string> scheduled;
		boost::mutex schedlock;
	} listener;
	rte1.addListener(&listener);
	struct : public CanDirectoryListenerExecuter {
		void execute(CanDirectoryListener& l)
		{
			string home;
			homeTrashDirectory(home);
			l.home(home);
		}
	} homeTrashCan;
	std::string trashname1("/tmp/mySchedulable");
	BindNotifier bn(rte1);
	homeTrashCan.execute(bn);
	ChangeNotifier cn(rte2,homeTrashCan);

	this_thread::sleep(posix_time::seconds(1));
	int i=5000;
	while(i--) {
		ostringstream o;
		o << "/tmp/mySchedulable";
		o << i;
		{
			boost::mutex::scoped_lock lock(listener.schedlock);
			listener.scheduled.insert(o.str());
			cn.schedule(o.str());
		}
	}
	this_thread::sleep(posix_time::seconds(10));
	{
		ofstream leftover("/tmp/trailer-leftover.log");
		set<string>::iterator left=listener.scheduled.begin();
		while(left!=listener.scheduled.end()) {
			leftover << *left++ << endl;
		}
	}
	BOOST_CHECK(listener.scheduled.size()==0);
//	BOOST_CHECK_EQUAL(listener.trash, listener.scheduled.back());
	rte1.shutdown();
	rte2.shutdown();
}

void testConnectionKey()
{
	remote::ConnectionKey k1("mybase","myhost1",2);
	remote::ConnectionKey k2("mybase","myhost1",3);
	BOOST_CHECK(k1<k2);
	BOOST_CHECK(!(k2<k1));
}

test_suite* init_unit_test_suite( int argc, char* argv[] )
{
	test_suite* com = BOOST_TEST_SUITE( "Communication" );
	//com->add( BOOST_TEST_CASE( &testConnectionKey ) );
	com->add( BOOST_TEST_CASE( &testRemoteDirectoryUpdater ) );
	//com->add( BOOST_TEST_CASE( &testRemoteTrashEvent ) );

	framework::master_test_suite().add( com );
	return 0;
}
