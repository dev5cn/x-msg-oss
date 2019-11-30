/*
  Copyright 2019 www.dev5.cn, Inc. dev5@qq.com
 
  This file is part of X-MSG-IM.
 
  X-MSG-IM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  X-MSG-IM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU Affero General Public License
  along with X-MSG-IM.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libx-msg-oss-core.h>
#include <libx-msg-oss-db.h>
#include <libx-msg-oss-msg.h>
#include "XmsgOss.h"

XmsgOss* XmsgOss::inst = new XmsgOss();

XmsgOss::XmsgOss()
{

}

XmsgOss* XmsgOss::instance()
{
	return XmsgOss::inst;
}

bool XmsgOss::start(const char* path)
{
	Log::setInfo();
	shared_ptr<XmsgOssCfg> cfg = XmsgOssCfg::load(path); 
	if (cfg == nullptr)
		return false;
	Log::setLevel(cfg->cfgPb->log().level().c_str());
	Log::setOutput(cfg->cfgPb->log().output());
	Xsc::init();
	if (!XmsgOssDb::instance()->load())
		return false;
	if (XmsgOssCfg::instance()->cfgPb->misc().storage() == XmsgOssStorageType::X_MSG_OSS_STORAGE_TYPE_IPFS && !XmsgOssIpfsOper::instance()->init()) 
		return false;
	XmsgOssTransmissionMgr::instance()->init();
	shared_ptr<XscTcpServer> tcpServer(new XscTcpServer(cfg->cfgPb->cgt(), shared_ptr<XmsgOssTcpLog>(new XmsgOssTcpLog())));
	if (!tcpServer->startup(XmsgOssCfg::instance()->priXscTcpServerCfg())) 
		return false;
	shared_ptr<XscHttpServer> httpServer(new XscHttpServer(cfg->cgt->toString(), shared_ptr<XmsgOssHttpLog>(new XmsgOssHttpLog())));
	if (!httpServer->startup(cfg->pubXscHttpServerCfg())) 
		return false;
	shared_ptr<XmsgImN2HMsgMgr> priMsgMgr(new XmsgImN2HMsgMgr(tcpServer));
	XmsOssMsg::init(priMsgMgr);
	if (!tcpServer->publish()) 
		return false;
	if (!httpServer->publish()) 
		return false;
	if (!this->connect2ne(tcpServer))
		return false;
	Xsc::hold([](ullong now)
	{
		XmsgOssInfoMgr::instance()->dida(now);
	});
	return true;
}

bool XmsgOss::connect2ne(shared_ptr<XscTcpServer> tcpServer)
{
	for (int i = 0; i < XmsgOssCfg::instance()->cfgPb->h2n_size(); ++i)
	{
		auto& ne = XmsgOssCfg::instance()->cfgPb->h2n(i);
		if (ne.neg() == X_MSG_AP)
		{
			shared_ptr<XmsgAp> ap(new XmsgAp(tcpServer, ne.addr(), ne.pwd(), ne.alg()));
			ap->connect();
			continue;
		}
		if (ne.neg() == X_MSG_IM_HLR)
		{
			shared_ptr<XmsgImHlr> hlr(new XmsgImHlr(tcpServer, ne.addr(), ne.pwd(), ne.alg()));
			hlr->connect();
			continue;
		}
		if (ne.neg() == X_MSG_IM_GROUP)
		{
			shared_ptr<XmsgImGroup> group(new XmsgImGroup(tcpServer, ne.addr(), ne.pwd(), ne.alg()));
			group->connect();
			continue;
		}
		LOG_ERROR("unsupported network element group: %s", ne.ShortDebugString().c_str())
		return false;
	}
	return true;
}

XmsgOss::~XmsgOss()
{

}

