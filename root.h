#ifndef SETUP_ROOT_H
#define SETUP_ROOT_H

#include "proppage.h"

class RootPage:public PropertyPage
{
public:
  RootPage ()
  {
  };
  virtual ~ RootPage ()
  {
  };

  bool Create ();

  virtual void OnInit ();
  virtual long OnNext ();
  virtual long OnBack ();
  virtual long OnUnattended ();
};

#endif /* SETUP_ROOT_H */
