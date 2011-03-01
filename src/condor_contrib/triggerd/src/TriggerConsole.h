/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _TRIGGERCONSOLE_H
#define _TRIGGERCONSOLE_H

#include <list>

#include "qpid/console/ConsoleListener.h"
#include "qpid/console/SessionManager.h"

namespace com {
namespace redhat {
namespace grid {

using namespace qpid::console;
using namespace com::redhat::grid;

class TriggerConsole : public ConsoleListener {
   private:
      SessionManager* sm;
      Broker* broker;

   public:
      TriggerConsole();
      ~TriggerConsole();

      std::list<std::string> findAbsentNodes();
      void config(std::string host, int port, std::string user, std::string passwd, std::string methods = "ANONYMOUS");
};

}}} /* com::redhat::grid */

#endif /* _TRIGGERCONSOLE_H */
