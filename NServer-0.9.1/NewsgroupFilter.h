/*
 * Herbert Straub: February 2002
 * 	add wildmat search in NewscacheFilter
*/
#ifndef __NewsgroupFilter_h__
#define __NewsgroupFilter_h__

#include <string>

#include "Newsgroup.h"
#include "wildmat.h"

/**
 * \class NewsgroupFilter
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class NewsgroupFilter {
  private:
	std::string rulelist;
	const char *c_rulelist;
	std::string Wildmat;
	int WildmatSearch;

	class RuleIterator {
		const char *rulelist, *rulelistp;
		char rule[1 + MAXNEWSGROUPNAMELEN];	// '!'+MAXNEWSGROUPNAMELEN

	  public:
		enum { iter_begin, iter_end };

		RuleIterator()
			: rulelist(NULL), rulelistp(NULL)
		{
			rule[0] = '\0';
		}

		RuleIterator(const char *rulelist, int pos)
			: rulelist(rulelist)
		{
			if (pos == iter_begin) {
				char *rulep = rule;
				char c;
				 rulelistp = rulelist;
				while ((c = *rulelistp) && c != ',') {
					*rulep = c;
					++rulelistp;
					++rulep;
				} *rulep = '\0';
			} else {
				rulelistp = NULL;
			}
		}

		RuleIterator & operator++() {	//prefix
			char *rulep = rule;
			char c;
			if (*rulelistp) {
				++rulelistp;
				while ((c = *rulelistp) && c != ',') {
					*rulep = c;
					++rulelistp;
					++rulep;
				}
				*rulep = '\0';
			} else {
				rulelistp = NULL;
			}

			return *this;
		}

		const char *operator*() {
			return rule;
		}

		int operator==(const RuleIterator & iter2) {
			return rulelistp == iter2.rulelistp;
		}

		int operator!=(const RuleIterator & iter2) {
			return rulelistp != iter2.rulelistp;
		}

		const RuleIterator & operator=(const RuleIterator & iter2) {
			rulelistp = iter2.rulelistp;
			strcpy(rule, iter2.rule);
			return *this;
		}
	};

	/**
	 * Add a rule to a list of rules if the rule is not already contained
	 * in the list of rules. 
	 * \param rulelist The list of rules.
	 * \param rule Rule to be added.
	 */
	void add_rule_to_rulelist(std::string & rulelist, const char *rule) {
		const char *p = rulelist.c_str(), *q;

		do {
			q = rule;
			while (*p == *q) {
				++p;
				++q;
			}
			if (*q == '\0' && (*p == '\0' || *p == ',')) {
				// current rule already contained in rulelist
				return;
			}
			while (*p && *p++ != ',');
		} while (*p);

		rulelist += rule;
		rulelist += ',';
	}

	RuleIterator begin() const
	{
		return RuleIterator(c_rulelist, RuleIterator::iter_begin);
	}

	RuleIterator end() const
	{
		return RuleIterator(c_rulelist, RuleIterator::iter_end);
	}

  public:
	NewsgroupFilter()
		: WildmatSearch(0)
	{
		c_rulelist = rulelist.c_str();
	}

	/**
	 * Construct a newsgroup filter based on a list of rules.
	 * E.g., at.*,!at.top.secret.*
	 * \param filter The initial filter
	 */
	NewsgroupFilter(const NewsgroupFilter & filter)
		: rulelist(filter.rulelist), WildmatSearch(0)
	{
		this->c_rulelist = this->rulelist.c_str();
	}

	/**
	 * Construct a newsgroup filter based on a list of rules.
	 * E.g., at.*,!at.top.secret.*
	 * \param rulelist Is a comma separated list of newsgroup 
	 * rules.  Rules may start with a '!' to indicate a reject
	 * rule and may end with the '*' wildcard.
	 */
	NewsgroupFilter(const char *rulelist)
		: rulelist(rulelist), WildmatSearch(0)
	{
		this->c_rulelist = this->rulelist.c_str();
	}

	NewsgroupFilter & operator=(const NewsgroupFilter & filter) {
		this->rulelist = filter.rulelist;
		this->c_rulelist = rulelist.c_str();
		this->WildmatSearch = filter.WildmatSearch;
		if (this->WildmatSearch) {
			this->Wildmat = filter.Wildmat;
		}
		return *this;
	}

	NewsgroupFilter & operator=(const char *rulelist) {
		this->rulelist = rulelist;
		this->c_rulelist = this->rulelist.c_str();
		return *this;
	}

	NewsgroupFilter & operator=(const std::string & rulelist) {
		this->rulelist = rulelist;
		this->c_rulelist = this->rulelist.c_str();
		return *this;
	}

	int operator==(const char *rulelist) {
		return this->rulelist == rulelist;
	}

	/**
	 * Set Wildmat Pattern. Using the wildmat routine from Rich Salz
	 * INN Project.
	 * \author Herbert Straub
	 * \param pWildmat Wildmat Pattern. Do shell-style pattern
	 * 	matching for ?, \, [], and * characters.
	 */
	void setWildmat(const char *pWildmat) {
		Wildmat = pWildmat;
		WildmatSearch = 1;
	}

	/**
	 * returning the current rulelist
	 * \author Herbert Straub
	 * \return rulelist
	 */
	const std::string& getRulelist (void) {
		return rulelist;
	}

	/**
	 * Check whether a given newsgroup is matched/rejected by this
	 * filter.  The result is based on the longest rule matching
	 * the newsgroup. Wildmat Search Extend by Herbert Straub.
	 * \param newsgroup The newsgroup to be matched.
	 * \return 
	 * 	\li If the newsgroup is accepted by the filter, the
	 * 		number of matching characters plus one is returned.
	 * 	\li If the newsgroup is rejected by the filter, the number
	 * 		of matching characters minus one will be returned.
	 */
	int matches(const char *newsgroup) const {
		char c;
		int j, clen, bmlen = 0, bmrej = 0;
		int rejrule = 0;

		 j = 0;
		do {
			clen = 0;
			if (c_rulelist[j] == '!') {
				j++;
				rejrule = 1;
			} else {
				rejrule = 0;
			}
			while ((c = c_rulelist[j]) && c == newsgroup[clen]) {
				clen++;
				j++;
			}

			switch (c) {
			case ',':
			case '\0':
				if (newsgroup[clen] != '\0'
				    && !isspace(newsgroup[clen])) {
					clen = -2;
				}
				clen++;
				break;
			case '*':
				clen++;
				break;
			default:
				clen = -1;
				break;
			}

			if (clen > bmlen) {
				bmlen = clen;
				bmrej = rejrule;
			}

			while (c && c != ',') {
				j++;
				c = c_rulelist[j];
			}
			j++;
		} while (c);

		if (bmrej) {
			return -bmlen;
		} else if (!WildmatSearch) {
			return bmlen;
		} else {
			return (wildmat(newsgroup, Wildmat.c_str())? (1)
				: (0));
		}
		//return bmrej?-bmlen:bmlen;
	}

	/**
	 * new filter matches all groups matched by current filter or by
	 * filter2.
	 * 	\li Remove all accept rules of filter2 already accepted
	 * 		by current filter. 
	 * 	\li Remove all reject rules accepted by other filter.
	 * \param filter2 The other filter.
	 */
	NewsgroupFilter & operator|=(const NewsgroupFilter & filter2) {
		std::string new_rulelist;
		RuleIterator begin, end;
		const char *grp;

		begin = this->begin();
		end = this->end();
		while (begin != end) {
			// copy all accept rules
			// copy reject rules also rejected by filter2
			if ((grp = *begin)[0] != '!'
			    || filter2.matches(grp + 1) <= 0) {
				add_rule_to_rulelist(new_rulelist, grp);
			}
			++begin;
		}

		begin = filter2.begin();
		end = filter2.end();
		while (begin != end) {
			// do not copy accept/reject rules of filter2 accepted by this filter
			if ((grp = *begin)[0] != '!') {
				if (matches(grp) <= 0) {
					add_rule_to_rulelist(new_rulelist,
							     grp);
				}
			} else if (matches(grp + 1) <= 0) {
				add_rule_to_rulelist(new_rulelist, grp);
			}
			++begin;
		}

		long l;
		if ((l = new_rulelist.length()) > 0) {
			new_rulelist.replace(l - 1, l, "");
		}
		rulelist = new_rulelist;
		c_rulelist = rulelist.c_str();
		return *this;
	}

	/**
	 * new filter matches all groups matched by current filter and 
	 * by filter2.
	 * \li Remove all reject rules of filter2 already rejected by
	 * 	current filter
	 * \li Remove all accept rules rejected by other filter
	 * \param filter2 The other filter.
	 */
	NewsgroupFilter & operator&=(const NewsgroupFilter & filter2) {
		std::string new_rulelist;
		RuleIterator begin, end;
		const char *grp;

		begin = this->begin();
		end = this->end();
		while (begin != end) {
			// copy all reject rules
			// copy accept rules not rejected by filter2
			if ((grp = *begin)[0] == '!'
			    || filter2.matches(grp) > 0) {
				add_rule_to_rulelist(new_rulelist, grp);
			}
			++begin;
		}

		begin = filter2.begin();
		end = filter2.end();
		while (begin != end) {
			// do not add positive/negative rules already rejected
			if ((grp = *begin)[0] != '!') {
				if (matches(grp) > 0) {
					add_rule_to_rulelist(new_rulelist,
							     grp);
				}
			} else if (matches(grp + 1) > 0) {
				add_rule_to_rulelist(new_rulelist, grp);
			}
			++begin;
		}

		long l;
		if ((l = new_rulelist.length()) > 0) {
			new_rulelist.replace(l - 1, l, "");
		}
		rulelist = new_rulelist;
		c_rulelist = rulelist.c_str();
		return *this;
	}

	friend std::ostream & operator <<(std::ostream & os,
					  const NewsgroupFilter & f) {
		os << f.rulelist;
		return os;
	}
};

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
