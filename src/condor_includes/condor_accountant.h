#ifndef _Condor_Accountant_H_
#define _Condor_Accountant_H_

#include "condor_classad.h"

#include "HashTable.h"
#include "MyString.h"
#include "Set.h"
#include "TimeClass.h"

class Accountant {

public:

  // User Functions

  void SetAccountant(char* sin); // set accountant remote/local mode
  void Reset();                  // Flush all tables

  double GetPriority(const MyString& CustomerName); // get priority for a customer
  void SetPriority(const MyString& CustomerName, double Priority); // set priority for a customer

  void AddMatch(const MyString& CustomerName, ClassAd* ResourceAd); // Add new match
  void RemoveMatch(const MyString& ResourceName); // remove a match

  void CheckMatches(ClassAdList& ResourceList);  // Remove matches that are not claimed
  void UpdatePriorities(); // update all the priorities

  void LoadPriorities(); // Save to file
  void SavePriorities(); // Read from file

  //----------------------------------------------------------------------

  static int HashFunc(const MyString& Key, int TableSize) {
    int count=0;
    int length=Key.Length();
    for(int i=0; i<length; i++) count+=Key[i];
    return (count % TableSize);
  }
  
  Accountant(int MaxCustomers=1024, int MaxResources=1024);
                                                
private:

  //---------------------------------------------
  // Data structures & members
  //---------------------------------------------

  typedef Set<MyString> StringSet;
  struct CustomerRecord {
    double Priority;
    double UnchargedTime;
    StringSet ResourceNames;
    CustomerRecord() { Priority=UnchargedTime=0; }
  };

  typedef HashTable<MyString, CustomerRecord*> CustomerTable;
  CustomerTable Customers;

  struct ResourceRecord {
    MyString CustomerName;
    ClassAd* Ad;
    Time StartTime;
    ResourceRecord() { Ad=NULL; }
    ~ResourceRecord() { if (Ad) delete Ad; }
  };

  typedef HashTable<MyString, ResourceRecord*> ResourceTable;
  ResourceTable Resources;

  // Configuration variables

  double MinPriority;
  double Epsilon;
  double HalfLifePeriod; // The time in sec in which the priority is halved by aging
  Time LastUpdateTime;
  MyString PriorityFile; // Name of priority file

  //--------------------------------------------------------
  // Misc functions
  //--------------------------------------------------------

  MyString GetResourceName(ClassAd* Resource);
  int NotClaimed(ClassAd* ResourceAd);

};

#endif
