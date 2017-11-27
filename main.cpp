#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <algorithm>
#include <cassert>
#include <jsoncpp/json/reader.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/StreamCopier.h>
#include <Poco/InflatingStream.h>
#include "file.hpp"


using std::string;
using std::map;
using std::set;
using std::istream;
using std::ifstream;
using std::ofstream;
using std::cerr;
using std::vector;
using std::ostringstream;
using std::istringstream;
using std::pair;
using std::sort;
namespace Net = Poco::Net;
using Net::HTTPResponse;
using MemberId = int;
using GroupId = int;
using VenueId = int;


template <typename T>
static string str(const T& arg)
{
  ostringstream stream;
  stream << arg;
  return stream.str();
}


static MemberId asMemberId(const Json::Value &value)
{
  return value.asInt();
}


inline GroupId asGroupId(const Json::Value &value)
{
  return value.asInt();
}


inline VenueId asVenueId(const Json::Value &value)
{
  return value.asInt();
}


static int asInt(const string &arg)
{
  istringstream stream(arg);
  int value = 0;
  if (!(stream >> value)) {
    cerr << "Can't convert to int: " << arg << "\n";
  }
  return value;
}


static int extractRateLimitReset(const HTTPResponse &response)
{
  const string &ratelimit_reset_string = response["X-RateLimit-Reset"];
  int reset_time_in_seconds = asInt(ratelimit_reset_string);
  return reset_time_in_seconds;
}


static void sleepUntilReset(const HTTPResponse &response)
{
  int reset_time_in_seconds = extractRateLimitReset(response);
  int sleep_time = reset_time_in_seconds+1;
  cerr << "Sleeping for " << sleep_time << " seconds.\n";
  sleep(sleep_time);
}


static string makeQuery(const string &path_and_query)
{
  for (;;) {
    Poco::URI uri("http://api.meetup.com");
    Net::HTTPClientSession session(uri.getHost(),uri.getPort());
    Net::HTTPRequest request(Net::HTTPRequest::HTTP_GET,path_and_query);
    HTTPResponse response;
    session.sendRequest(request);
    istream& response_stream = session.receiveResponse(response);
    HTTPResponse::HTTPStatus status = response.getStatus();

    if (status==429) {
      cerr << "Too many requests\n";
      cerr << "path_and_query: " << path_and_query << "\n";
      response.write(cerr);
      sleepUntilReset(response);
    }
    else if (status==HTTPResponse::HTTP_NOT_FOUND) {
      cerr << "Not found\n";
      cerr << "path_and_query: " << path_and_query << "\n";
      assert(false);
    }
    else {
      const string &ratelimit_remaining_string =
        response["X-RateLimit-Remaining"];
      cerr << "ratelimit_remaining_string: " << ratelimit_remaining_string << "\n";
      if (asInt(ratelimit_remaining_string)==0) {
        sleepUntilReset(response);
      }

      if (status!=HTTPResponse::HTTP_OK) {
        cerr << "status=" << status << "\n";
        return "";
      }

      ostringstream output_stream;
      Poco::StreamCopier::copyStream(response_stream,output_stream);
      string response_string = output_stream.str();
      return response_string;
    }
  }
}


static string groupByUrlnameQuery(const string &group_urlname)
{
  return "/"+group_urlname;
}


static Json::Value parseJson(const string &response_string)
{
  istringstream input_stream(response_string);
  Json::Reader reader;
  Json::Value root_value;

  if (!reader.parse(input_stream,root_value)) {
    return 0;
  }

  return root_value;
}


static int getGroupId(const string &group_urlname)
{
  string query = groupByUrlnameQuery(group_urlname);
  string response_string = makeQuery(query);
  Json::Value root_value = parseJson(response_string);
  return asGroupId(root_value["id"]);
}


static vector<MemberId> extractMemberIds(const string &members_string)
{
  Json::Value root_value = parseJson(members_string);
  const Json::Value &results_json = root_value["results"];
  Json::ArrayIndex n_members = results_json.size();
  vector<MemberId> member_ids;

  for (Json::ArrayIndex i=0; i!=n_members; ++i) {
    MemberId member_id = asMemberId(results_json[i]["id"]);
    member_ids.push_back(member_id);
  }

  return member_ids;
}


#if 0
static void printMemberCountsByCity(const string &members_string)
{
  Json::Value root_value = parseJson(members_string);
  const Json::Value &results_json = root_value["results"];
  Json::ArrayIndex n_members = results_json.size();
  map<string,int> city_counts;

  for (Json::ArrayIndex i=0; i!=n_members; ++i) {
    string city = results_json[i]["city"].asString();
    ++city_counts[city];
  }

  for (auto &key_value_pair : city_counts) {
    auto& key = key_value_pair.first;
    auto& value = key_value_pair.second;
    cerr << key << ": " << value << "\n";
  }
}
#endif


static string keyedQuery(const string &query,const string &user_key)
{
  return query+"&key="+user_key;
}


static string makeKeyedQuery(const string &query,const string &user_key)
{
  return makeQuery(keyedQuery(query,user_key));
}


static string membersByGroupIdQuery(GroupId group_id)
{
  return "/2/members?&group_id=" + str(group_id);
}


static string
  makeMembersByGroupIdQuery(GroupId group_id,const string &user_key)
{
  return makeKeyedQuery(membersByGroupIdQuery(group_id),user_key);
}


static string readFile(const string &path)
{
  ifstream input_stream(path);
  ostringstream output_stream;
  output_stream << input_stream.rdbuf() << "\n";
  return output_stream.str();
}


static string groupsByMemberIdQuery(MemberId member_id)
{
  return "/2/groups?&member_id="+str(member_id);
}


static void saveFile(const string &path,const string &text)
{
  ofstream output_stream(path);
  if (!output_stream) {
    cerr << "saveFile: Unable to open " << path << "\n";
    return;
  }
  output_stream << text;
  cerr << "Saved " << path << "\n";
}


static string queryCachePath(const string &query)
{
  return "cache/"+query;
}


static void saveToCache(const string &query,const string &response)
{
  string file_path = queryCachePath(query);
  createDirectories(directoryOf(file_path));
  saveFile(file_path,response);
}


static string readFromCache(const string &query)
{
  return readFile(queryCachePath(query));
}



#if 0
static string pastEventsForGroupUrlnameQuery(const string &group_urlname)
{
  return "/"+group_urlname+"/events?status=past";
}
#endif


static string venuesForGroupUrlnameQuery(const string &group_urlname)
{
  return "/2/venues?group_urlname="+group_urlname;
}


static string groupWithTopicsForUrlnameQuery(const string &group_urlname)
{
  return "/"+group_urlname+"?fields=topics";
}


static string
  makeCacheableKeyedQuery(const string &query,const string &user_key)
{
  {
    string response = readFromCache(query);

    if (response!="") {
      return response;
    }
  }
  {
    string response = makeKeyedQuery(query,user_key);
    saveToCache(query,response);
    return response;
  }
}


template <typename T>
static void sortDescending(vector<T> &arg)
{
  sort(arg.begin(),arg.end(),std::greater<>());
}


template <typename T>
static bool contains(const vector<T> &container,const T &item)
{
  auto iter = find(container.begin(),container.end(),item);
  return iter!=container.end();
}


#if 0
static string eventQuery(const string &group_urlname,const string &event_id)
{
  return "/"+group_urlname+"/events/"+event_id;
}
#endif


template <typename Key,typename Value>
static vector<pair<Value,Key>>
  makeSwappedPairVector(const map<Key,Value> &the_map)
{
  vector<pair<Value,Key>> result;

  for (auto &pair : the_map) {
    const Key &key = pair.first;
    const Value &value = pair.second;
    result.push_back(std::make_pair(value,key));
  }
  
  return result;
}


static vector<MemberId> lookupMemberIds(GroupId group_id,const string &user_key)
{
  string members_string = makeMembersByGroupIdQuery(group_id,user_key);
  
  vector<MemberId> member_ids = extractMemberIds(members_string);
  return member_ids;
}


template <typename Function>
static void
  forEachGroupOfMember(
    MemberId member_id,
    const string &user_key,
    const Function &f
  )
{
  string groups_query = groupsByMemberIdQuery(member_id);
  string groups_response = makeCacheableKeyedQuery(groups_query,user_key);
  Json::Value groups_response_json = parseJson(groups_response);
  const Json::Value &groups_results_json = groups_response_json["results"];

  for (auto &group_json : groups_results_json) {
    f(group_json);
  }
}


static string readStringFromFile(const string &path)
{
  ifstream stream(path);

  if (!stream) {
    cerr << "Unable top open " << path << "\n";
    exit(EXIT_FAILURE);
  }

  string result;
  stream >> result;
  return result;
}


int main()
{
  static string user_key = readStringFromFile("user_key.txt");
  string group_urlname = "Atlanta-CPP-Meetup";
  GroupId group_id = getGroupId(group_urlname);
  vector<MemberId> member_ids = lookupMemberIds(group_id,user_key);

  // Go through all the members and see what groups they are in.
  map<GroupId,int> group_counts;
  map<GroupId,string> group_urlnames;

  for (MemberId member_id : member_ids) {
    forEachGroupOfMember(member_id,user_key,[&](auto &group_json){
      GroupId group_id = asGroupId(group_json["id"]);
      ++group_counts[group_id];
      group_urlnames[group_id] = group_json["urlname"].asString();
    });
  }

  // Find groups that at least two of our members are members of and
  // are about one of these topics:
  vector<string> relevant_topic_urlnames = {
    "softwaredev",
    "computer-programming",
    "programming-languages",
  };

  set<GroupId> relevant_group_ids;

  for (auto &group_count_pair : group_counts) {
    GroupId group_id = group_count_pair.first;
    auto &count = group_count_pair.second;

    if (count>=2) {
      const string &group_urlname = group_urlnames[group_id];
      string group_query = groupWithTopicsForUrlnameQuery(group_urlname);
      string group_respose = makeCacheableKeyedQuery(group_query,user_key);
      Json::Value group_json = parseJson(group_respose);
      const Json::Value &topics_json = group_json["topics"];

      for (auto &topic_json : topics_json) {
        const string &topic_urlkey = topic_json["urlkey"].asString();
        if (contains(relevant_topic_urlnames,topic_urlkey)) {
          relevant_group_ids.insert(group_id);
        }
      }
    }
  }

  // Get information about the venues used by the relevant groups.
  map<VenueId,int> venue_counts;
  map<VenueId,string> venue_names;
  map<VenueId,string> venue_addresses;
  map<VenueId,float> venue_ratings;
  map<VenueId,vector<GroupId>> venue_group_ids;

  for (GroupId group_id : relevant_group_ids) {
    const string &group_urlname = group_urlnames[group_id];
    string venues_query = venuesForGroupUrlnameQuery(group_urlname);
    string response = makeCacheableKeyedQuery(venues_query,user_key);
    Json::Value root_value = parseJson(response);

    for (auto &venue_value : root_value["results"]) {
      const string &venue_name = venue_value["name"].asString();
      float venue_rating = venue_value["rating"].asFloat();
      const string &venue_address_1 = venue_value["address_1"].asString();
      const string &venue_address_2 = venue_value["address_2"].asString();
      VenueId venue_id = asVenueId(venue_value["id"]);
      venue_names[venue_id] = venue_name;
      venue_ratings[venue_id] = venue_rating;
      venue_addresses[venue_id] = venue_address_1+";"+venue_address_2;
      venue_group_ids[venue_id].push_back(group_id);
      ++venue_counts[venue_id];
    }
  }

  // Show the top 10 venues.
  vector<pair<int,VenueId>> venues_by_count =
    makeSwappedPairVector(venue_counts);
  sortDescending(venues_by_count);
  venues_by_count.resize(10);

  for (auto &count_venue_pair : venues_by_count) {
    VenueId venue_id = count_venue_pair.second;

    cerr << venue_names[venue_id] << "\n";
    cerr << "  rating: " << venue_ratings[venue_id] << "\n";
    cerr << "  address: " << venue_addresses[venue_id] << "\n";
    cerr << "  groups:\n";
    for (auto &group_id : venue_group_ids[venue_id]) {
      cerr << "    " << group_urlnames[group_id] << "\n";
    }
  }
}
