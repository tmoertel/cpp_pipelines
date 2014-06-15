// Tests for uniform interface for protocol buffers.
// Tom Moertel <tom@moertel.com>


#include <set>
#include <string>
#include <vector>

#include "../../consumers_and_producers.h"
#include "example.pb.h"
#include "gtest/gtest.h"

using std::set;
using std::string;
using std::vector;

namespace example {

template<typename T>
Producer<T*> Produce(google::protobuf::RepeatedPtrField<T>* ts) {
  return {
    [&ts](Consumer<T*> c) {
      for (auto& t : *ts) {
        c(&t);
      }
    }
  };
}

template<typename T>
Producer<const T*> Produce(const google::protobuf::RepeatedPtrField<T>& ts) {
  return {
    [&ts](Consumer<const T*> c) {
      for (auto& t : ts) {
        c(&t);
      }
    }
  };
}

// Produce the teams within a company.
static Filter<const Company*, const Team*> CompanyTeams{
  [](const Company* p) { return Produce(p->teams()); }
};

// Produce the manager of a team if the team has one.
static Filter<const Team*, const Person*> TeamManager{
  [](const Team* p) {
    return p->has_manager() ? MUnit(&p->manager()) : PZero<const Person*>();
  }
};

// Produce the members of a team.
static Filter<const Team*, const Person*> TeamMembers{
  [](const Team* p) { return Produce(p->members()); }
};

// Produce the name of a team if it has one.
static Filter<const Team*, const string*> TeamName{
  [](const Team* p) {
    return p->has_name() ? MUnit(&p->name()) : PZero<const string*>();
  }
};

// Produce the name of a person.
static Filter<const Person*, const string*> PersonName{
  [](const Person* p) { return MUnit(&p->name()); }
};


TEST(ProducersAndConsumers, Basics) {
  Company company;
  company.set_name("Test Company");
  {
    Team* team = company.add_teams();
    team->set_name("The Three Stooges");
    team->add_members()->set_name("Curly");
    team->add_members()->set_name("Larry");
    team->add_members()->set_name("Moe");
  }
  {
    Team* team = company.add_teams();
    team->set_name("The X-Men Lite");
    team->mutable_manager()->set_name("Prof. X");
    team->add_members()->set_name("Colossus");
    team->add_members()->set_name("Wolverine");
  }
  {
    Team* team = company.add_teams();
    // No team name.
    // No manager.
    team->add_members()->set_name("Lone Wolf McQuade");
  }

  {
    // Our test consumer will record all of the names it is given.
    vector<string> names;
    Consumer<const string*> add_to_names {
      [&](const string* name) { names.push_back(*name); }
    };

    // We can find all team names.
    (CompanyTeams * TeamName)(&company)(add_to_names);
    EXPECT_EQ(vector<string>({"The Three Stooges", "The X-Men Lite"}),
              names);

    // We can find the names of all members of all teams.
    names.clear();
    (CompanyTeams * TeamMembers * PersonName)(&company)(add_to_names);
    EXPECT_EQ(vector<string>({"Curly", "Larry", "Moe",
                              "Colossus", "Wolverine",
                              "Lone Wolf McQuade"}),
              names);

    // We can find the names of all managers, too.
    names.clear();
    (CompanyTeams * TeamManager * PersonName)(&company)(add_to_names);
    EXPECT_EQ(vector<string>({"Prof. X"}), names);

    // Yes, we can find the names of all managers *and* team members.
    names.clear();
    auto filter = CompanyTeams * (TeamManager + TeamMembers) * PersonName;
    filter(&company)(add_to_names);
    EXPECT_EQ(vector<string>({"Curly", "Larry", "Moe",
                              "Prof. X",  // Manager.
                              "Colossus", "Wolverine",
                              "Lone Wolf McQuade"}),
              names);

    // As you might expect, * is right-distributive over +. Therefore,
    // the following filter must produce the same results as the previous.
    vector<string> previous_names = names;  // Expect same as last time.
    filter = CompanyTeams * (TeamManager * PersonName +
                             TeamMembers * PersonName);
    names.clear();
    filter(&company)(add_to_names);
    EXPECT_EQ(previous_names, names);

    // But * is not left-distributive over +: output order may change.
    filter = (CompanyTeams * TeamManager +  // Want all team managers first.
              CompanyTeams * TeamMembers    // And then all team members.
             ) * PersonName;
    names.clear();
    filter(&company)(add_to_names);
    EXPECT_EQ(vector<string>({"Prof. X",  // Manager.
                              "Curly", "Larry", "Moe",
                              "Colossus", "Wolverine",
                              "Lone Wolf McQuade"}),
              names);
  }
}

}  // example


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
