/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <sparta/PatriciaTreeSet.h>

#include <mariana-trench/GroupHashedSetAbstractDomain.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class GroupHashedSetAbstractDomainTest : public test::Test {};

namespace {

using IntSet = sparta::PatriciaTreeSet<unsigned>;

struct Element {
  int group;
  IntSet values;

  bool is_bottom() const {
    return values.empty();
  }

  void set_to_bottom() {
    values.clear();
  }

  bool operator==(const Element& other) const {
    return group == other.group && values == other.values;
  }

  bool leq(const Element& other) const {
    return group == other.group && values.is_subset_of(other.values);
  }

  void join_with(const Element& other) {
    values.union_with(other.values);
  }

  struct GroupHash {
    std::size_t operator()(const Element& element) const {
      return element.group;
    }
  };

  struct GroupEqual {
    bool operator()(const Element& left, const Element& right) const {
      return left.group == right.group;
    }
  };

  friend std::ostream& operator<<(std::ostream& o, const Element& element) {
    return o << "Element(group=" << element.group
             << ", values=" << element.values << ")";
  }
};

using AbstractDomainT = GroupHashedSetAbstractDomain<
    Element,
    Element::GroupHash,
    Element::GroupEqual>;

} // namespace

TEST_F(GroupHashedSetAbstractDomainTest, DefaultConstructor) {
  EXPECT_TRUE(AbstractDomainT().is_bottom());
  EXPECT_TRUE(AbstractDomainT().empty());
  EXPECT_TRUE(AbstractDomainT().size() == 0);
}

TEST_F(GroupHashedSetAbstractDomainTest, Add) {
  auto domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10}}};

  EXPECT_TRUE(domain.size() == 1);

  domain.add(Element{/* group */ 2, /* values */ IntSet{}});

  EXPECT_TRUE(domain.size() == 1);

  domain.add(Element{/* group */ 2, /* values */ IntSet{20}});

  EXPECT_TRUE(domain.size() == 2);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10}},
          Element{/* group */ 2, /* values */ IntSet{20}})));

  domain.add(Element{/* group */ 1, /* values */ IntSet{12}});

  EXPECT_TRUE(domain.size() == 2);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{20}})));

  domain.add(Element{/* group */ 2, /* values */ IntSet{20, 21, 22}});

  EXPECT_TRUE(domain.size() == 2);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{20, 21, 22}})));

  domain.add(Element{/* group */ 3, /* values */ IntSet{30}});

  EXPECT_TRUE(domain.size() == 3);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{20, 21, 22}},
          Element{/* group */ 3, /* values */ IntSet{30}})));
}

TEST_F(GroupHashedSetAbstractDomainTest, LessOrEqual) {
  EXPECT_TRUE(AbstractDomainT::bottom().leq(AbstractDomainT::bottom()));
  EXPECT_TRUE(AbstractDomainT().leq(AbstractDomainT::bottom()));

  EXPECT_TRUE(AbstractDomainT::bottom().leq(AbstractDomainT()));
  EXPECT_TRUE(AbstractDomainT().leq(AbstractDomainT()));

  auto domain1 =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 12}}};
  EXPECT_FALSE(domain1.leq(AbstractDomainT::bottom()));
  EXPECT_FALSE(domain1.leq(AbstractDomainT{}));
  EXPECT_TRUE(AbstractDomainT::bottom().leq(domain1));
  EXPECT_TRUE(AbstractDomainT().leq(domain1));
  EXPECT_TRUE(domain1.leq(domain1));

  EXPECT_TRUE(
      (AbstractDomainT{
           Element{/* group */ 1, /* values */ IntSet{11}},
       })
          .leq(
              AbstractDomainT{
                  Element{/* group */ 1, /* values */ IntSet{10, 11, 12}},
              }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{11}},
                })
                   .leq(
                       AbstractDomainT{
                           Element{/* group */ 1, /* values */ IntSet{10, 12}},
                       }));

  EXPECT_TRUE((AbstractDomainT{
                   Element{/* group */ 1, /* values */ IntSet{11}},
               })
                  .leq(
                      AbstractDomainT{
                          Element{
                              /* group */ 1,
                              /* values */ IntSet{10, 11, 12},
                          },
                          Element{
                              /* group */ 2,
                              /* values */ IntSet{20},
                          },
                      }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{11}},
                    Element{/* group */ 2, /* values */ IntSet{21}},
                })
                   .leq(
                       AbstractDomainT{
                           Element{
                               /* group */ 1,
                               /* values */ IntSet{10, 11, 12},
                           },
                       }));

  EXPECT_TRUE((AbstractDomainT{
                   Element{/* group */ 1, /* values */ IntSet{11}},
                   Element{/* group */ 2, /* values */ IntSet{21}},
               })
                  .leq(
                      AbstractDomainT{
                          Element{
                              /* group */ 1,
                              /* values */ IntSet{10, 11, 12},
                          },
                          Element{
                              /* group */ 2,
                              /* values */ IntSet{20, 21, 22},
                          },
                      }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{11}},
                    Element{/* group */ 2, /* values */ IntSet{20, 21, 23}},
                })
                   .leq(
                       AbstractDomainT{
                           Element{
                               /* group */ 1,
                               /* values */ IntSet{10, 11, 12},
                           },
                           Element{
                               /* group */ 2,
                               /* values */ IntSet{20, 21, 22},
                           },
                       }));

  EXPECT_TRUE((AbstractDomainT{
                   Element{/* group */ 1, /* values */ IntSet{12, 11, 10}},
                   Element{/* group */ 2, /* values */ IntSet{22, 21, 20}},
               })
                  .leq(
                      AbstractDomainT{
                          Element{
                              /* group */ 1,
                              /* values */ IntSet{10, 11, 12},
                          },
                          Element{
                              /* group */ 2,
                              /* values */ IntSet{20, 21, 22},
                          },
                          Element{
                              /* group */ 3,
                              /* values */ IntSet{},
                          },
                      }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{12, 11, 10}},
                    Element{/* group */ 2, /* values */ IntSet{22, 21, 20}},
                    Element{/* group */ 4, /* values */ IntSet{0}},
                })
                   .leq(
                       AbstractDomainT{
                           Element{
                               /* group */ 1,
                               /* values */ IntSet{10, 11, 12},
                           },
                           Element{
                               /* group */ 2,
                               /* values */ IntSet{20, 21, 22},
                           },
                           Element{
                               /* group */ 3,
                               /* values */ IntSet{0},
                           },
                       }));
}

TEST_F(GroupHashedSetAbstractDomainTest, Equals) {
  EXPECT_TRUE(AbstractDomainT::bottom().equals(AbstractDomainT::bottom()));
  EXPECT_TRUE(AbstractDomainT().equals(AbstractDomainT::bottom()));

  EXPECT_TRUE(AbstractDomainT::bottom().equals(AbstractDomainT()));
  EXPECT_TRUE(AbstractDomainT().equals(AbstractDomainT()));

  auto domain1 =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 12}}};
  EXPECT_FALSE(domain1.equals(AbstractDomainT::bottom()));
  EXPECT_FALSE(domain1.equals(AbstractDomainT{}));
  EXPECT_FALSE(AbstractDomainT::bottom().equals(domain1));
  EXPECT_FALSE(AbstractDomainT().equals(domain1));
  EXPECT_TRUE(domain1.equals(domain1));

  EXPECT_TRUE((AbstractDomainT{
                   Element{/* group */ 1, /* values */ IntSet{11}},
               })
                  .equals(
                      AbstractDomainT{
                          Element{/* group */ 1, /* values */ IntSet{11}},
                      }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{11}},
                })
                   .equals(
                       AbstractDomainT{
                           Element{/* group */ 1, /* values */ IntSet{12}},
                       }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{11}},
                })
                   .equals(
                       AbstractDomainT{
                           Element{
                               /* group */ 1,
                               /* values */ IntSet{11},
                           },
                           Element{
                               /* group */ 2,
                               /* values */ IntSet{20},
                           },
                       }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{11}},
                    Element{/* group */ 2, /* values */ IntSet{21}},
                })
                   .equals(
                       AbstractDomainT{
                           Element{
                               /* group */ 1,
                               /* values */ IntSet{11},
                           },
                       }));

  EXPECT_TRUE((AbstractDomainT{
                   Element{/* group */ 1, /* values */ IntSet{10, 11, 12}},
                   Element{/* group */ 2, /* values */ IntSet{20, 21, 22}},
               })
                  .equals(
                      AbstractDomainT{
                          Element{
                              /* group */ 1,
                              /* values */ IntSet{10, 11, 12},
                          },
                          Element{
                              /* group */ 2,
                              /* values */ IntSet{20, 21, 22},
                          },
                      }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{11}},
                    Element{/* group */ 2, /* values */ IntSet{20, 21, 23}},
                })
                   .equals(
                       AbstractDomainT{
                           Element{
                               /* group */ 1,
                               /* values */ IntSet{11},
                           },
                           Element{
                               /* group */ 2,
                               /* values */ IntSet{20, 21, 22},
                           },
                       }));

  EXPECT_TRUE((AbstractDomainT{
                   Element{/* group */ 1, /* values */ IntSet{12, 11, 10}},
                   Element{/* group */ 2, /* values */ IntSet{22, 21, 20}},
               })
                  .equals(
                      AbstractDomainT{
                          Element{
                              /* group */ 1,
                              /* values */ IntSet{10, 11, 12},
                          },
                          Element{
                              /* group */ 2,
                              /* values */ IntSet{20, 21, 22},
                          },
                      }));

  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{12, 11, 10}},
                    Element{/* group */ 2, /* values */ IntSet{22, 21, 20}},
                    Element{/* group */ 4, /* values */ IntSet{0}},
                })
                   .equals(
                       AbstractDomainT{
                           Element{
                               /* group */ 1,
                               /* values */ IntSet{10, 11, 12},
                           },
                           Element{
                               /* group */ 2,
                               /* values */ IntSet{20, 21, 22},
                           },
                           Element{
                               /* group */ 3,
                               /* values */ IntSet{0},
                           },
                       }));
}

TEST_F(GroupHashedSetAbstractDomainTest, JoinWith) {
  auto domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10}}};

  EXPECT_TRUE(domain.size() == 1);

  domain.join_with(
      AbstractDomainT{Element{/* group */ 2, /* values */ IntSet{20}}});

  EXPECT_TRUE(domain.size() == 2);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10}},
          Element{/* group */ 2, /* values */ IntSet{20}})));

  domain.join_with(
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{12}}});

  EXPECT_TRUE(domain.size() == 2);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{20}})));

  domain.join_with(
      AbstractDomainT{Element{/* group */ 2, /* values */ IntSet{20, 21, 22}}});

  EXPECT_TRUE(domain.size() == 2);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{20, 21, 22}})));

  domain.join_with(
      AbstractDomainT{Element{/* group */ 3, /* values */ IntSet{30}}});

  EXPECT_TRUE(domain.size() == 3);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{20, 21, 22}},
          Element{/* group */ 3, /* values */ IntSet{30}})));

  domain = AbstractDomainT();
  domain.join_with(
      AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10}},
          Element{/* group */ 2, /* values */ IntSet{20}},
      });
  EXPECT_TRUE(domain.size() == 2);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10}},
          Element{/* group */ 2, /* values */ IntSet{20}})));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 3, /* values */ IntSet{20, 22}},
  };
  domain.join_with(
      AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{11, 13}},
          Element{/* group */ 2, /* values */ IntSet{0}},
          Element{/* group */ 3, /* values */ IntSet{21, 23}},
      });
  EXPECT_TRUE(domain.size() == 3);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12, 13}},
          Element{/* group */ 2, /* values */ IntSet{0}},
          Element{/* group */ 3, /* values */ IntSet{20, 21, 22, 23}})));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 3, /* values */ IntSet{20, 22}},
  };
  domain.join_with(
      AbstractDomainT{
          Element{/* group */ 2, /* values */ IntSet{11, 13}},
      });
  EXPECT_TRUE(domain.size() == 3);
  EXPECT_THAT(
      domain,
      (testing::UnorderedElementsAre(
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{11, 13}},
          Element{/* group */ 3, /* values */ IntSet{20, 22}})));
}

TEST_F(GroupHashedSetAbstractDomainTest, Contains) {
  EXPECT_TRUE(
      AbstractDomainT{}.contains(
          Element{/* group */ 1, /* values */ IntSet{}}));
  EXPECT_FALSE(
      AbstractDomainT{}.contains(
          Element{/* group */ 1, /* values */ IntSet{10}}));
  EXPECT_TRUE(
      (AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}})
          .contains(Element{/* group */ 1, /* values */ IntSet{}}));
  EXPECT_TRUE(
      (AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}})
          .contains(Element{/* group */ 1, /* values */ IntSet{10}}));
  EXPECT_TRUE(
      (AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}})
          .contains(Element{/* group */ 1, /* values */ IntSet{10, 12}}));
  EXPECT_FALSE(
      (AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 12}}})
          .contains(Element{/* group */ 1, /* values */ IntSet{11}}));
  EXPECT_TRUE(
      (AbstractDomainT{
           Element{/* group */ 1, /* values */ IntSet{10, 11, 12}},
           Element{/* group */ 2, /* values */ IntSet{20}}})
          .contains(Element{/* group */ 1, /* values */ IntSet{10, 12}}));
  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{10, 12}},
                    Element{/* group */ 2, /* values */ IntSet{11}}})
                   .contains(Element{/* group */ 1, /* values */ IntSet{11}}));
  EXPECT_TRUE((AbstractDomainT{
                   Element{/* group */ 1, /* values */ IntSet{10, 12}},
                   Element{/* group */ 2, /* values */ IntSet{11}}})
                  .contains(Element{/* group */ 2, /* values */ IntSet{11}}));
  EXPECT_FALSE((AbstractDomainT{
                    Element{/* group */ 1, /* values */ IntSet{10, 12}},
                    Element{/* group */ 3, /* values */ IntSet{11}}})
                   .contains(Element{/* group */ 2, /* values */ IntSet{11}}));
}

TEST_F(GroupHashedSetAbstractDomainTest, Remove) {
  auto domain = AbstractDomainT{};

  domain.remove(Element{/* group */ 1, /* values */ IntSet{}});
  EXPECT_EQ(domain, AbstractDomainT{});

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.remove(Element{/* group */ 1, /* values */ IntSet{10}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}}));

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.remove(Element{/* group */ 1, /* values */ IntSet{10, 11, 12, 13}});
  EXPECT_EQ(domain, AbstractDomainT{});

  domain = AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 12}}};
  domain.remove(Element{/* group */ 1, /* values */ IntSet{11}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 12}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.remove(Element{/* group */ 1, /* values */ IntSet{10, 11}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{11}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.remove(Element{/* group */ 1, /* values */ IntSet{10, 12}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{Element{/* group */ 2, /* values */ IntSet{11}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 3, /* values */ IntSet{11}}};
  domain.remove(Element{/* group */ 2, /* values */ IntSet{11}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 3, /* values */ IntSet{11}}}));
}

TEST_F(GroupHashedSetAbstractDomainTest, Difference) {
  auto domain = AbstractDomainT{};
  domain.difference_with(
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{}}});
  EXPECT_EQ(domain, AbstractDomainT{});

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.difference_with(
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10}}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}}));

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.difference_with(
      AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12, 13}}});
  EXPECT_EQ(domain, AbstractDomainT{});

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.difference_with(
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11}}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 2, /* values */ IntSet{11}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.difference_with(
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{Element{/* group */ 2, /* values */ IntSet{11}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 3, /* values */ IntSet{11}}};
  domain.difference_with(
      AbstractDomainT{Element{/* group */ 2, /* values */ IntSet{11}}});
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 12}},
          Element{/* group */ 3, /* values */ IntSet{11}}}));

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.difference_with(
      AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10}},
          Element{/* group */ 2, /* values */ IntSet{20}},
      });
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}}));

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.difference_with(
      AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12, 13}},
          Element{/* group */ 2, /* values */ IntSet{20}},
      });
  EXPECT_EQ(domain, AbstractDomainT{});
}

TEST_F(GroupHashedSetAbstractDomainTest, Transform) {
  auto domain = AbstractDomainT{};

  domain.transform([](Element element) {
    element.values.insert(20);
    return element;
  });
  EXPECT_EQ(domain, AbstractDomainT{});

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.transform([](Element element) {
    element.values.insert(20);
    return element;
  });
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12, 20}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.transform([](Element element) {
    element.values.insert(20);
    return element;
  });
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 12, 20}},
          Element{/* group */ 2, /* values */ IntSet{11, 20}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.transform([](Element element) {
    element.values.clear();
    return element;
  });
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{}},
          Element{/* group */ 2, /* values */ IntSet{}}}));
}

TEST_F(GroupHashedSetAbstractDomainTest, Filter) {
  auto domain = AbstractDomainT{};

  domain.filter([](const Element& element) { return element.group == 1; });
  EXPECT_EQ(domain, AbstractDomainT{});

  domain =
      AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}};
  domain.filter([](const Element& element) { return element.group == 1; });
  EXPECT_EQ(
      domain,
      (AbstractDomainT{
          Element{/* group */ 1, /* values */ IntSet{10, 11, 12}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.filter([](const Element& element) { return element.group == 1; });
  EXPECT_EQ(
      domain,
      (AbstractDomainT{Element{/* group */ 1, /* values */ IntSet{10, 12}}}));

  domain = AbstractDomainT{
      Element{/* group */ 1, /* values */ IntSet{10, 12}},
      Element{/* group */ 2, /* values */ IntSet{11}}};
  domain.filter(
      [](const Element& element) { return element.values.size() <= 1; });
  EXPECT_EQ(
      domain,
      (AbstractDomainT{Element{/* group */ 2, /* values */ IntSet{11}}}));

  domain.filter([](const Element& element) { return element.group == 1; });
  EXPECT_EQ(domain, AbstractDomainT{});
}

} // namespace marianatrench
