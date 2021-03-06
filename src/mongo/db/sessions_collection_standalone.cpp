/**
 * Copyright (C) 2017 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/db/sessions_collection_standalone.h"

#include "mongo/client/dbclientinterface.h"
#include "mongo/client/query.h"
#include "mongo/db/dbdirectclient.h"
#include "mongo/db/operation_context.h"

namespace mongo {

namespace {

BSONObj lsidQuery(const LogicalSessionId& lsid) {
    return BSON(LogicalSessionRecord::kIdFieldName << lsid.toBSON());
}
}  // namespace

StatusWith<LogicalSessionRecord> SessionsCollectionStandalone::fetchRecord(
    OperationContext* opCtx, const LogicalSessionId& lsid) {
    DBDirectClient client(opCtx);
    auto cursor = client.query(kSessionsFullNS.toString(), lsidQuery(lsid), 1);
    if (!cursor->more()) {
        return {ErrorCodes::NoSuchSession, "No matching record in the sessions collection"};
    }

    try {
        IDLParserErrorContext ctx("LogicalSessionRecord");
        return LogicalSessionRecord::parse(ctx, cursor->next());
    } catch (...) {
        return exceptionToStatus();
    }
}

Status SessionsCollectionStandalone::refreshSessions(OperationContext* opCtx,
                                                     const LogicalSessionRecordSet& sessions,
                                                     Date_t refreshTime) {
    DBDirectClient client(opCtx);
    return doRefresh(sessions, refreshTime, makeSendFn(&client));
}

Status SessionsCollectionStandalone::removeRecords(OperationContext* opCtx,
                                                   const LogicalSessionIdSet& sessions) {
    DBDirectClient client(opCtx);
    return doRemove(sessions, makeSendFn(&client));
}

SessionsCollection::SendBatchFn SessionsCollectionStandalone::makeSendFn(DBDirectClient* client) {
    auto send = [client](BSONObj batch) -> Status {
        BSONObj res;
        auto ok = client->runCommand(SessionsCollection::kSessionsDb.toString(), batch, res);
        if (!ok) {
            return {ErrorCodes::UnknownError,
                    client->getLastError(SessionsCollection::kSessionsDb.toString())};
        }
        return Status::OK();
    };

    return send;
}


}  // namespace mongo
