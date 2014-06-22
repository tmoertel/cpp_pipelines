// Tests for accessor combinators for protocol buffers.
// Tom Moertel <tom@moertel.com>

#include <string>
#include <vector>

#include "../../consumers_and_producers.h"
#include "example.pb.h"
#include "gtest/gtest.h"

using std::string;
using std::vector;

// BEGIN GENERATED CODE -- The following code would be automatically
// generated from the example.proto file in a real implementation.
// Here I'm just writing it by hand to show the underlying formula.

using google::protobuf::RepeatedPtrField;
using example::Company;
using example::Person;
using example::Team;

// A read-write accessor for some underlying object of type A.
template <typename A> struct RW { const A& ro; A* rw; };

// A read-write filter that visits fields of type F on a probobuf of type P.
template <typename P, typename F> using RWFilter = Filter<RW<P>, RW<F>>;

// Generic proto accessors for common field types.
template <typename P>
struct _proto_accessors {
  // Accessors for reading, read-writing, and testing proto fields.
  template <typename F> using ROA = const F& (P::*)() const;
  template <typename F> using RWA = F* (P::*)();
  using TestA = bool (P::*)() const;

  // Methods for accessing fields via accessors.
  // Read.
  template <typename F>
  static inline const F& access(const P& p, ROA<F> roa) {
    return (p.*roa)();
  }
  // Read/write.
  template <typename F>
  static inline F* access(P* p, RWA<F> rwa) {
    // We access pointed-to protos only via this method to ensure that
    // null pointers are never dereferenced. Further, accessing a
    // field on a "null" proto always returns nullptr.
    return p ? (p->*rwa)() : nullptr;
  }
  // Test whether the field exists.
  static inline bool access(const P& p, TestA hasa) {
    return (p.*hasa)();
  }

  // The following methods provide a uniform interface for accessing
  // optional, required, and repeated protobuf fields of object types
  // (non-scalars like strings and embedded messages). They produce
  // filters that receive protobufs of type P and produce field values
  // of type F.
  template <typename F>
  static RWFilter<P, F> optional_obj(TestA hasa, ROA<F> roa, RWA<F> rwa) {
    return [=](const RW<P>& rwp) {
      return access(rwp.ro, hasa) ?
        PUnit<RW<F>>({access(rwp.ro, roa), access(rwp.rw, rwa)}) :
        PZero<RW<F>>();
    };
  }

  template <typename F>
  static RWFilter<P, F> required_obj(ROA<F> roa, RWA<F> rwa) {
    return [roa, rwa](const RW<P>& rwp) {
      return PUnit<RW<F>>({access(rwp.ro, roa), access(rwp.rw, rwa)});
    };
  }
  // For repeated fields, this method handles the common case: it
  // returns a filter that automatically traverses the
  // RepeatedPtrField that contains the field's objects. If you want
  // access to the RepeatedPtrField itself, just use required_obj to
  // produce it. Then you can process it as desired by chaining on
  // additional filters. The method itself is an example of the
  // technique.
  template <typename F>
  static RWFilter<P, F> repeated_obj(ROA<RepeatedPtrField<F>> roa,
                                     RWA<RepeatedPtrField<F>> rwa) {
    return required_obj(roa, rwa) * scan_ptrs<F>();
  }

  // Produce a filter that scans the objects within a RepeatedPtrField.
  template <typename F>
  static const RWFilter<RepeatedPtrField<F>, F>& scan_ptrs() {
    static const auto& filter = *new RWFilter<RepeatedPtrField<F>, F> {
      [](const RW<RepeatedPtrField<F>>& rwrpf) {
        return [=](const Consumer<const RW<F>&>& c) {
          int i = 0;
          for (const F& elem : rwrpf.ro) {
            c(RW<F>{elem, rwrpf.rw ? rwrpf.rw->Mutable(i) : nullptr});
            ++i;
          }
        };
      }
    };
    return filter;
  }
};

// Accessors for Company proto.
struct CompanyA {
  using Self = CompanyA;
  using P = Company;
  using PA = _proto_accessors<P>;
  template <typename Field> using PF = RWFilter<P, Field>;

  PF<string> name;
  PF<Team> teams;
  PF<RepeatedPtrField<Team>> teams_coll;

  static const Self& Use() {
    static const Self& rep = *new Self {
      PA::required_obj(&P::name, &P::mutable_name),
      PA::repeated_obj(&P::teams, &P::mutable_teams),
      PA::required_obj(&P::teams, &P::mutable_teams)
    };
    return rep;
  }
};

// Accessors for Team proto.
struct TeamA {
  using Self = TeamA;
  using P = Team;
  using PA = _proto_accessors<P>;
  template <typename Field> using PF = RWFilter<P, Field>;

  PF<Person> manager;
  PF<Person> members;
  PF<RepeatedPtrField<Person>> members_coll;
  PF<string> name;

  static const Self& Use() {
    static const Self& rep = *new Self {
      PA::optional_obj(&P::has_manager, &P::manager, &P::mutable_manager),
      PA::repeated_obj(&P::members, &P::mutable_members),
      PA::required_obj(&P::members, &P::mutable_members),
      PA::optional_obj(&P::has_name, &P::name, &P::mutable_name),
    };
    return rep;
  }
};

// Accessors for Person proto.
struct PersonA {
  using Self = PersonA;
  using P = Person;
  using PA = _proto_accessors<P>;
  template <typename Field> using PF = RWFilter<P, Field>;

  PF<string> name;

  static const Self& Use() {
    static const Self& rep = *new Self {
      PA::required_obj(&P::name, &P::mutable_name),
    };
    return rep;
  }
};

// Convert a read-write filter into a read-only filter.
template <typename P, typename F>
Filter<const P&, const F&> ReadOnly(const RWFilter<P, F>& rwfilt) {
  return [=](const P& p) {
    return [=](const Consumer<const F&>& roc) {
      Consumer<const RW<F>&> rwc = [&](const RW<F>& rwval) {
        roc(rwval.ro);
      };
      rwfilt(RW<P>{p, nullptr})(rwc);
    };
  };
};

// Convert a read-write filter into a mutable pointer filter.
template <typename P, typename F>
Filter<P*, F*> ReadWrite(const RWFilter<P, F>& rwfilt) {
  return [=](P* p) {
    if (!p) {
      return PZero<F*>();
    }
    return Producer<F*> {
      [=](const Consumer<F*>& mpc) {
        Consumer<const RW<F>&> rwc = [&](const RW<F>& rwval) {
          mpc(rwval.rw);
        };
        rwfilt(RW<P>{*p, p})(rwc);
      }
    };
  };
};

// END GENERATED CODE


// BEGIN THE GOOD STUFF! The point of the logic we've built up to now
// is to let us create sophisticated processing pipelines from simple,
// composable building blocks that are type safe and have formally
// guaranteed semantics that we can basically ignore because they're
// intuitive (once you understand the nature of the blocks). The
// underlying algebraic stuff ensures that there are no weird corner
// cases and that anything you build will work as expected.
//
// These tests are a short tutorial for the library. They take the
// protobuf combinators for a spin and show typical use cases.

TEST(ProtoAccessors, Basics) {
  // Set up a test company with three teams.
  Company company;
  company.set_name("Test Company");
  {
    Team* team = company.add_teams();
    team->set_name("The Three Stooges");
    // No team manager.
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

  // To examine our filters, let's create a consumer to record their output.
  vector<string> names;
  Consumer<const string&> add_to_names = [&](const string& name) {
    names.push_back(name);
  };

  // Now let's get convenient access to combinators for our protobufs.
  const CompanyA& c = CompanyA::Use();
  const TeamA& t = TeamA::Use();
  const PersonA& p = PersonA::Use();

  // Now, the tests!

  // Find the name of our company. Let's take it step by step.
  ReadOnly(c.name)    // Run the c.name filter in read-only mode...
      (company)       // on our test company...
      (add_to_names); // and send the output to our recorder.
  EXPECT_EQ(vector<string>({"Test Company"}), names);

  // We can find the names of a company's teams by chaining the
  // filter that gets a company's teams with the filter that gets a
  // team's name. Think of the * operator as a type-safe version of
  // the command-line shell's pipe operator |. (The reason I use *
  // instead of | in this implementation is because there's also a +
  // operator over filters, and * and + interact as you might expect.)
  names.clear();  // Clear our recorder for new test case.
  ReadOnly(c.teams * t.name)(company)(add_to_names);
  EXPECT_EQ(vector<string>({"The Three Stooges", "The X-Men Lite"}), names);

  // Let's factor out the common part of running a read-only filter test.
  auto RunTestRO = [&](const RWFilter<Company, string>& filter,
                       const vector<string>& expected) {
    names.clear();
    ReadOnly(filter)(company)(add_to_names);
    EXPECT_EQ(expected, names);
  };

  // We can find the names of the company's teams' members.
  RunTestRO(c.teams * t.members * p.name,
            {"Curly", "Larry", "Moe",
             "Colossus", "Wolverine",
             "Lone Wolf McQuade"});

  // We can find the names of the company's teams' managers *and*
  // members.  Here we use the + operator to serially join the results
  // of two filters having the same input and output types. You can
  // read the chain as follows: Get the company's teams, and then for
  // each team get its manager and its members, and finally for each
  // person get that person's name.
  RunTestRO(c.teams * (t.manager + t.members) * p.name,
            {"Curly", "Larry", "Moe",
             "Prof. X",  // Manager of X-Men Lite.
             "Colossus", "Wolverine",
             "Lone Wolf McQuade"});

  // The * operator is right distributive over +.
  RunTestRO(c.teams * (t.manager * p.name + t.members * p.name),
            // Exact same results as before.
            {"Curly", "Larry", "Moe",
             "Prof. X",
             "Colossus", "Wolverine",
             "Lone Wolf McQuade"});

  // The * operator is fully distributive over + if you don't care about order.
  RunTestRO(c.teams * t.manager * p.name +  // Managers' names first.
            c.teams * t.members * p.name,   // Then member's names.
            {"Prof. X",  // Now the sole manager is first.
             "Curly", "Larry", "Moe",
             "Colossus", "Wolverine",
             "Lone Wolf McQuade"});

  // Now let's run a read-write filter that unmasks X-Men.
  // First, we create a mutating consumer that makes the changes we want.
  Consumer<string*> unmask_xmen = [&](string* name) {
    if (*name == "Colossus") {
      *name = "Piotr Rasputin";
    } else if (*name == "Prof. X") {
      *name = "Charles Xavier";
    } else if (*name == "Wolverine") {
      *name = "James 'Logan' Howlett";
    }
  };
  // Then we apply the consumer just to managers' names.
  const auto& manager_names_filter = c.teams * t.manager * p.name;
  ReadWrite(manager_names_filter)(&company)(unmask_xmen);
  // We can run the same filter in read-only mode to verify that
  // Prof. X -- the only manager and also an X-Man -- must now be
  // revealed as Charles Xavier.
  RunTestRO(manager_names_filter, {"Charles Xavier"});
  // Let's look at all names just to make sure we changed only Xavier's.
  RunTestRO(c.teams * (t.manager + t.members) * p.name,
            {"Curly", "Larry", "Moe",
             "Charles Xavier",  // Unmasked!
             "Colossus", "Wolverine",
             "Lone Wolf McQuade"});
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
