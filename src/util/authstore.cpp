/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "authstore.h"

#include "cpl_http.h"

#include "api_priv.h"
#include "util/error.h"
#include "util/notify.h"

namespace ngs {

void AuthNotifyFunction(const char* url, CPLHTTPAuthChangeCode operation)
{
    if(operation == HTTPAUTH_EXPIRED) {
        Notify::instance().onNotify(url, CC_TOKEN_EXPIRED);
    }
    else if(operation == HTTPAUTH_UPDATE) {
        Notify::instance().onNotify(url, CC_TOKEN_CHANGED);
    }
}

//------------------------------------------------------------------------------
// AuthStore
//------------------------------------------------------------------------------

bool AuthStore::addAuth(const char *url, const Options &options)
{
    auto optionsPtr = options.getOptions();
    if(CPLHTTPAuthAdd(url, optionsPtr.get(), AuthNotifyFunction) == CE_None) {
        return true;
    }
    return false;
}

void AuthStore::deleteAuth(const char* url)
{
    CPLHTTPAuthDelete(url);
}

Options AuthStore::description(const char *url)
{
    return Options(CPLHTTPAuthProperties(url));
}

} // namespace ngs
