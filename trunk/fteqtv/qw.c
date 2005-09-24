/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "qtv.h"


typedef struct {
	unsigned char msec;
	unsigned short angles[3];
	short forwardmove, sidemove, upmove;
	unsigned char buttons;
	unsigned char impulse;
} usercmd_t;
const usercmd_t nullcmd;

#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE3 	(1<<1)
#define	CM_FORWARD	(1<<2)
#define	CM_SIDE		(1<<3)
#define	CM_UP		(1<<4)
#define	CM_BUTTONS	(1<<5)
#define	CM_IMPULSE	(1<<6)
#define	CM_ANGLE2 	(1<<7)
void ReadDeltaUsercmd (netmsg_t *m, const usercmd_t *from, usercmd_t *move)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = ReadByte (m);
		
// read current angles
	if (bits & CM_ANGLE1)
		move->angles[0] = ReadShort (m);
	if (bits & CM_ANGLE2)
		move->angles[1] = ReadShort (m);
	if (bits & CM_ANGLE3)
		move->angles[2] = ReadShort (m);
		
// read movement
	if (bits & CM_FORWARD)
		move->forwardmove = ReadShort(m);
	if (bits & CM_SIDE)
		move->sidemove = ReadShort(m);
	if (bits & CM_UP)
		move->upmove = ReadShort(m);
	
// read buttons
	if (bits & CM_BUTTONS)
		move->buttons = ReadByte (m);

	if (bits & CM_IMPULSE)
		move->impulse = ReadByte (m);

// read time to run command
	move->msec = ReadByte (m);		// always sent
}

void WriteDeltaUsercmd (netmsg_t *m, const usercmd_t *from, usercmd_t *move)
{
	int bits = 0;

	if (move->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (move->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;
	if (move->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;

	if (move->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (move->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (move->upmove != from->upmove)
		bits |= CM_UP;

	if (move->buttons != from->buttons)
		bits |= CM_BUTTONS;
	if (move->impulse != from->impulse)
		bits |= CM_IMPULSE;


	WriteByte (m, bits);

// read current angles
	if (bits & CM_ANGLE1)
		WriteShort (m, move->angles[0]);
	if (bits & CM_ANGLE2)
		WriteShort (m, move->angles[1]);
	if (bits & CM_ANGLE3)
		WriteShort (m, move->angles[2]);
		
// read movement
	if (bits & CM_FORWARD)
		WriteShort(m, move->forwardmove);
	if (bits & CM_SIDE)
		WriteShort(m, move->sidemove);
	if (bits & CM_UP)
		WriteShort(m, move->upmove);
	
// read buttons
	if (bits & CM_BUTTONS)
		WriteByte (m, move->buttons);

	if (bits & CM_IMPULSE)
		WriteByte (m, move->impulse);

// read time to run command
	WriteByte (m, move->msec);		// always sent
}










SOCKET QW_InitUDPSocket(int port)
{
	int sock;

	struct sockaddr_in	address;
//	int fromlen;

	unsigned long nonblocking = true;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((u_short)port);



	if ((sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		return INVALID_SOCKET;
	}

	if (ioctlsocket (sock, FIONBIO, &nonblocking) == -1)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

	if( bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}
	return sock;
}

void BuildServerData(sv_t *tv, netmsg_t *msg, qboolean mvd)
{
	WriteByte(msg, svc_serverdata);
	WriteLong(msg, PROTOCOL_VERSION);
	WriteLong(msg, tv->servercount);
	WriteString(msg, tv->gamedir);

	if (mvd)
		WriteFloat(msg, 0);
	else
		WriteByte(msg, MAX_CLIENTS-1);
	WriteString(msg, tv->mapname);


	// get the movevars
	WriteFloat(msg, tv->movevars.gravity);
	WriteFloat(msg, tv->movevars.stopspeed);
	WriteFloat(msg, tv->movevars.maxspeed);
	WriteFloat(msg, tv->movevars.spectatormaxspeed);
	WriteFloat(msg, tv->movevars.accelerate);
	WriteFloat(msg, tv->movevars.airaccelerate);
	WriteFloat(msg, tv->movevars.wateraccelerate);
	WriteFloat(msg, tv->movevars.friction);
	WriteFloat(msg, tv->movevars.waterfriction);
	WriteFloat(msg, tv->movevars.entgrav);



	WriteByte(msg, svc_stufftext);
	WriteString2(msg, "fullserverinfo \"");
	WriteString2(msg, tv->serverinfo);
	WriteString(msg, "\"\n");
}

void SendServerData(sv_t *tv, viewer_t *viewer)
{
	netmsg_t msg;
	char buffer[1024];
	InitNetMsg(&msg, buffer, sizeof(buffer));
	
	BuildServerData(tv, &msg, false);

	SendBufferToViewer(viewer, msg.data, msg.cursize, true);

	viewer->thinksitsconnected = false;

	memset(viewer->currentstats, 0, sizeof(viewer->currentstats));
}

int SendCurrentUserinfos(sv_t *tv, int cursize, netmsg_t *msg, int i)
{
	if (i < 0)
		return i;
	if (i >= MAX_CLIENTS)
		return i;


	for (; i < MAX_CLIENTS-1; i++)
	{
		if (msg->cursize+cursize+strlen(tv->players[i].userinfo) > 768)
		{
			return i;
		}
		WriteByte(msg, svc_updateuserinfo);
		WriteByte(msg, i);
		WriteLong(msg, i);
		WriteString(msg, tv->players[i].userinfo);
	}

	WriteByte(msg, svc_updateuserinfo);
	WriteByte(msg, MAX_CLIENTS-1);
	WriteLong(msg, MAX_CLIENTS-1);
	WriteString(msg, "\\*spectator\\1\\name\\YOU!");

	i++;

	return i;
}
void WriteEntityState(netmsg_t *msg, entity_state_t *es)
{
	int i;
	WriteByte(msg, es->modelindex);
	WriteByte(msg, es->frame);
	WriteByte(msg, es->colormap);
	WriteByte(msg, es->skinnum);
	for (i = 0; i < 3; i++)
	{
		WriteShort(msg, es->origin[i]);
		WriteByte(msg, es->angles[i]);
	}
}
int SendCurrentBaselines(sv_t *tv, int cursize, netmsg_t *msg, int i)
{

	if (i < 0 || i >= MAX_ENTITIES)
		return i;

	for (; i < MAX_ENTITIES; i++)
	{
		if (msg->cursize+cursize+16 > 768)
		{
			return i;
		}
		WriteByte(msg, svc_spawnbaseline);
		WriteShort(msg, i);
		WriteEntityState(msg, &tv->entity[i].baseline);
	}

	return i;
}
int SendCurrentLightmaps(sv_t *tv, int cursize, netmsg_t *msg, int i)
{
	if (i < 0 || i >= MAX_LIGHTSTYLES)
		return i;

	for (; i < MAX_LIGHTSTYLES; i++)
	{
		if (msg->cursize+cursize+strlen(tv->lightstyle[i].name) > 768)
		{
			return i;
		}
		WriteByte(msg, svc_lightstyle);
		WriteByte(msg, i);
		WriteString(msg, tv->lightstyle[i].name);
	}
	return i;
}
int SendStaticSounds(sv_t *tv, int cursize, netmsg_t *msg, int i)
{
	if (i < 0 || i >= MAX_STATICSOUNDS)
		return i;

	for (; i < MAX_STATICSOUNDS; i++)
	{
		if (msg->cursize+cursize+16 > 768)
		{
			return i;
		}
		if (!tv->staticsound[i].soundindex)
			continue;

		WriteByte(msg, svc_spawnstaticsound);
		WriteShort(msg, tv->staticsound[i].origin[0]);
		WriteShort(msg, tv->staticsound[i].origin[1]);
		WriteShort(msg, tv->staticsound[i].origin[2]);
		WriteByte(msg, tv->staticsound[i].soundindex);
		WriteByte(msg, tv->staticsound[i].volume);
		WriteByte(msg, tv->staticsound[i].attenuation);
	}

	return i;
}
int SendStaticEntities(sv_t *tv, int cursize, netmsg_t *msg, int i)
{
	if (i < 0 || i >= MAX_STATICENTITIES)
		return i;

	for (; i < MAX_STATICENTITIES; i++)
	{
		if (msg->cursize+cursize+16 > 768)
		{
			return i;
		}
		if (!tv->spawnstatic[i].modelindex)
			continue;

		WriteByte(msg, svc_spawnstatic);
		WriteEntityState(msg, &tv->spawnstatic[i]);
	}

	return i;
}

int SendList(sv_t *qtv, int first, filename_t *list, int svc, netmsg_t *msg)
{
	int i;

	WriteByte(msg, svc);
	WriteByte(msg, first);
	for (i = first+1; i < 256; i++)
	{
//		printf("write %i: %s\n", i, list[i].name);
		WriteString(msg, list[i].name);
		if (!*list[i].name)	//fixme: this probably needs testing for where we are close to the limit
		{	//no more
			WriteByte(msg, 0);
			return -1;
		}

		if (msg->cursize > 768)
		{	//truncate
			i--;
			break;
		}
	}
	WriteByte(msg, 0);
	WriteByte(msg, i);

	return i;
}

//fixme: will these want to have state?..
int NewChallenge(netadr_t *addr)
{
	return 4;
}
qboolean ChallengePasses(netadr_t *addr, int challenge)
{
	if (challenge == 4)
		return true;
	return false;
}

void NewQWClient(sv_t *qtv, netadr_t *addr, char *connectmessage)
{
	viewer_t *viewer;

	char qport[32];
	char challenge[32];
	char infostring[256];

	connectmessage+=11;

	connectmessage = COM_ParseToken(connectmessage, qport, sizeof(qport), "");
	connectmessage = COM_ParseToken(connectmessage, challenge, sizeof(challenge), "");
	connectmessage = COM_ParseToken(connectmessage, infostring, sizeof(infostring), "");

	if (!ChallengePasses(addr, atoi(challenge)))
	{
		Netchan_OutOfBandPrint(qtv->qwdsocket, *addr, "n" "Bad challenge");
		return;
	}


	viewer = malloc(sizeof(viewer_t));
	memset(viewer, 0, sizeof(viewer_t));

	viewer->trackplayer = -1;
	Netchan_Setup (qtv->qwdsocket, &viewer->netchan, *addr, atoi(qport));

	viewer->next = qtv->viewers;
	qtv->viewers = viewer;
	viewer->delta_frame = -1;

	Info_ValueForKey(infostring, "name", viewer->name, sizeof(viewer->name));

	Netchan_OutOfBandPrint(qtv->qwdsocket, *addr, "j");
}

void QTV_Rcon(sv_t *qtv, char *message, netadr_t *from)
{
	char buffer[8192];

	char *command;
	int passlen;

	if (!*qtv->password)
	{
		Netchan_OutOfBandPrint(qtv->qwdsocket, *from, "n" "Rcon is disabled. Set a password.\n");
		return;
	}

	while(*message > '\0' && *message <= ' ')
		message++;
	
	command = strchr(message, ' ');
	passlen = command-message;
	if (passlen != strlen(qtv->password) || strncmp(message, qtv->password, passlen))
	{
		Netchan_OutOfBandPrint(qtv->qwdsocket, *from, "n" "Rcon password is incorrect\n");
		return;
	}

	Netchan_OutOfBandPrint(qtv->qwdsocket, *from, "n%s", Rcon_Command(qtv, command, buffer, sizeof(buffer), false));
}
void QTV_Status(sv_t *qtv, netadr_t *from)
{
	char buffer[8192];

	netmsg_t msg;
	char buf[6];
	InitNetMsg(&msg, buffer, sizeof(buffer));
	WriteLong(&msg, -1);
	WriteByte(&msg, 'n');
	WriteString2(&msg, qtv->serverinfo);
	WriteString(&msg, "\n");
	NET_SendPacket(qtv->qwdsocket, msg.cursize, msg.data, *from);


	//Netchan_OutOfBandPrint(qtv->qwdsocket, *from, "n%s\n", qtv->serverinfo);
}


void ConnectionlessPacket(sv_t *qtv, netadr_t *from, netmsg_t *m)
{
	char buffer[MAX_MSGLEN];
	int i;

	ReadLong(m);
	ReadString(m, buffer, sizeof(buffer));
	
	if (!strncmp(buffer, "rcon ", 5))
	{
		QTV_Rcon(qtv, buffer+5, from);
		return;
	}
	if (!strncmp(buffer, "status", 6))
	{
		QTV_Status(qtv, from);
		return;
	}
	if (!strncmp(buffer, "getchallenge", 12))
	{
		i = NewChallenge(from);
		Netchan_OutOfBandPrint(qtv->qwdsocket, *from, "c%i", i);
		return;
	}
	if (!strncmp(buffer, "connect 28 ", 11))
	{
		if (qtv->sourcesock == INVALID_SOCKET && !qtv->file)	//tell them what's going on instead of expecting them to see a lack of anything happening.
			Netchan_OutOfBandPrint(qtv->qwdsocket, *from, "n" "Proxy is not connected to a server\n");
		else if (qtv->parsingconnectiondata)	//connecting at this time is a bit silly.
			Netchan_OutOfBandPrint(qtv->qwdsocket, *from, "n" "Buffering demo, please try again\n");
		else
			NewQWClient(qtv, from, buffer);
		return;
	}
	if (!strncmp(buffer, "l\n", 2))
	{
		printf("Ack\n");
	}
}


void SV_WriteDelta(int entnum, const entity_state_t *from, const entity_state_t *to, netmsg_t *msg, qboolean force)
{
	unsigned int i;
	unsigned int bits;

	bits = 0;
	if (from->angles[0] != to->angles[0])
		bits |= U_ANGLE1;
	if (from->angles[1] != to->angles[1])
		bits |= U_ANGLE2;
	if (from->angles[2] != to->angles[2])
		bits |= U_ANGLE3;

	if (from->origin[0] != to->origin[0])
		bits |= U_ORIGIN1;
	if (from->origin[1] != to->origin[1])
		bits |= U_ORIGIN2;
	if (from->origin[2] != to->origin[2])
		bits |= U_ORIGIN3;

	if (from->colormap != to->colormap)
		bits |= U_COLORMAP;
	if (from->skinnum != to->skinnum)
		bits |= U_SKIN;
	if (from->modelindex != to->modelindex)
		bits |= U_MODEL;
	if (from->frame != to->frame)
		bits |= U_FRAME;
	if (from->effects != to->effects)
		bits |= U_EFFECTS;

	if (bits & 255)
		bits |= U_MOREBITS;



	if (!bits && !force)
		return;

	i = (entnum&511) | (bits&~511);
	WriteShort (msg, i);

	if (bits & U_MOREBITS)
		WriteByte (msg, bits&255);
/*
#ifdef PROTOCOLEXTENSIONS
	if (bits & U_EVENMORE)
		WriteByte (msg, evenmorebits&255);
	if (evenmorebits & U_YETMORE)
		WriteByte (msg, (evenmorebits>>8)&255);
#endif
*/
	if (bits & U_MODEL)
		WriteByte (msg,	to->modelindex&255);
	if (bits & U_FRAME)
		WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		WriteByte (msg, to->skinnum);
	if (bits & U_EFFECTS)
		WriteByte (msg, to->effects&0x00ff);
	if (bits & U_ORIGIN1)
		WriteShort (msg, to->origin[0]);
	if (bits & U_ANGLE1)
		WriteByte(msg, to->angles[0]);
	if (bits & U_ORIGIN2)
		WriteShort (msg, to->origin[1]);
	if (bits & U_ANGLE2)
		WriteByte(msg, to->angles[1]);
	if (bits & U_ORIGIN3)
		WriteShort (msg, to->origin[2]);
	if (bits & U_ANGLE3)
		WriteByte(msg, to->angles[2]);
}

const entity_state_t nullentstate;
void SV_EmitPacketEntities (const sv_t *qtv, const viewer_t *v, const packet_entities_t *to, netmsg_t *msg)
{
	const entity_state_t *baseline;
	const packet_entities_t *from;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		oldmax;

	// this is the frame that we are going to delta update from
	if (v->delta_frame != -1)
	{
		from = &v->frame[v->delta_frame & (ENTITY_FRAMES-1)];
		oldmax = from->numents;

		WriteByte (msg, svc_deltapacketentities);
		WriteByte (msg, v->delta_frame);
	}
	else
	{
		oldmax = 0;	// no delta update
		from = NULL;

		WriteByte (msg, svc_packetentities);
	}

	newindex = 0;
	oldindex = 0;
//Con_Printf ("---%i to %i ----\n", client->delta_sequence & UPDATE_MASK
//			, client->netchan.outgoing_sequence & UPDATE_MASK);
	while (newindex < to->numents || oldindex < oldmax)
	{
		newnum = newindex >= to->numents ? 9999 : to->entnum[newindex];
		oldnum = oldindex >= oldmax ? 9999 : from->entnum[oldindex];

		if (newnum == oldnum)
		{	// delta update from old position
//Con_Printf ("delta %i\n", newnum);
			SV_WriteDelta (newnum, &from->ents[oldindex], &to->ents[newindex], msg, false);

			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			baseline = &qtv->entity[newnum].baseline;
//Con_Printf ("baseline %i\n", newnum);
			SV_WriteDelta (newnum, baseline, &to->ents[newindex], msg, true);			

			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
//Con_Printf ("remove %i\n", oldnum);
			WriteShort (msg, oldnum | U_REMOVE);
			oldindex++;
			continue;
		}
	}

	WriteShort (msg, 0);	// end of packetentities
}

void Prox_SendInitialEnts(sv_t *qtv, oproxy_t *prox, netmsg_t *msg)
{
	int i;
	WriteByte(msg, svc_packetentities);
	for (i = 0; i < qtv->maxents; i++)
		SV_WriteDelta(i, &nullentstate, &qtv->entity[i].current, msg, true);
	WriteShort(msg, 0);
}

static float InterpolateAngle(float current, float ideal, float fraction)
{
	float move;

	move = ideal - current;
	if (move >= 32767)
		move -= 65535;
	else if (move <= -32767)
		move += 65535;

	return current + fraction * move;
}

void SendPlayerStates(sv_t *tv, viewer_t *v, netmsg_t *msg)
{
	packet_entities_t *e;
	int i;
	usercmd_t to;
	unsigned short flags;
	short interp;
	float lerp;

	if (tv->physicstime != v->settime && tv->chokeonnotupdated)
	{
		WriteByte(msg, svc_updatestatlong);
		WriteByte(msg, STAT_TIME);
		WriteLong(msg, v->settime);

		v->settime = tv->physicstime;
	}


	memset(&to, 0, sizeof(to));

	/*if (v->trackplayer>=0)
	{
		BSP_SetupForPosition(tv->bsp,	tv->players[v->trackplayer].current.origin[0]/8.0f,
										tv->players[v->trackplayer].current.origin[1]/8.0f,
										tv->players[v->trackplayer].current.origin[2]/8.0f);
	}
	else*/
	{
		BSP_SetupForPosition(tv->bsp, v->origin[0], v->origin[1], v->origin[2]);
	}
		
	lerp = ((tv->curtime - tv->oldpackettime)/1000.0f) / ((tv->nextpackettime - tv->oldpackettime)/1000.0f);
	if (lerp < 0)
		lerp = 0;
	if (lerp > 1)
		lerp = 1;


	for (i = 0; i < MAX_CLIENTS-1; i++)
	{
		if (!tv->players[i].active)
			continue;

		if (v->trackplayer != i && !BSP_Visible(tv->bsp, tv->players[i].leafcount, tv->players[i].leafs))
			continue;

		flags = PF_COMMAND;
		if (v->trackplayer == i && tv->players[i].current.weaponframe)
			flags |= PF_WEAPONFRAME;

		WriteByte(msg, svc_playerinfo);
		WriteByte(msg, i);
		WriteShort(msg, flags);

		interp = (lerp)*tv->players[i].current.origin[0] + (1-lerp)*tv->players[i].old.origin[0];
		WriteShort(msg, interp);
		interp = (lerp)*tv->players[i].current.origin[1] + (1-lerp)*tv->players[i].old.origin[1];
		WriteShort(msg, interp);
		interp = (lerp)*tv->players[i].current.origin[2] + (1-lerp)*tv->players[i].old.origin[2];
		WriteShort(msg, interp);

		WriteByte(msg, tv->players[i].current.frame);

		if (flags & PF_MSEC)
		{
			WriteByte(msg, 0);
		}
		if (flags & PF_COMMAND)
		{
//			to.angles[0] = tv->players[i].current.angles[0];
//			to.angles[1] = tv->players[i].current.angles[1];
//			to.angles[2] = tv->players[i].current.angles[2];

			to.angles[0] = InterpolateAngle(tv->players[i].old.angles[0], tv->players[i].current.angles[0], lerp);
			to.angles[1] = InterpolateAngle(tv->players[i].old.angles[1], tv->players[i].current.angles[1], lerp);
			to.angles[2] = InterpolateAngle(tv->players[i].old.angles[2], tv->players[i].current.angles[2], lerp);
			WriteDeltaUsercmd(msg, &nullcmd, &to);
		}
		//vel
		//model
		//skin
		//effects
		//weaponframe
		if (flags & PF_WEAPONFRAME)
			WriteByte(msg, tv->players[i].current.weaponframe);
	}

	WriteByte(msg, svc_playerinfo);
	WriteByte(msg, MAX_CLIENTS-1);
	WriteShort(msg, 0);

	WriteShort(msg, v->origin[0]*8);
	WriteShort(msg, v->origin[1]*8);
	WriteShort(msg, v->origin[2]*8);

	WriteByte(msg, 0);





	e = &v->frame[v->netchan.outgoing_sequence&(ENTITY_FRAMES-1)];
	e->numents = 0;
	for (i = 0; i < tv->maxents; i++)
	{
		if (!tv->entity[i].current.modelindex)
			continue;

		if (tv->entity[i].current.modelindex >= tv->numinlines && !BSP_Visible(tv->bsp, tv->entity[i].leafcount, tv->entity[i].leafs))
			continue;

		e->entnum[e->numents] = i;
		memcpy(&e->ents[e->numents], &tv->entity[i].current, sizeof(entity_state_t));

		if (tv->entity[i].updatetime == tv->oldpackettime)
		{
			e->ents[e->numents].origin[0] = (lerp)*tv->entity[i].current.origin[0] + (1-lerp)*tv->entity[i].old.origin[0];
			e->ents[e->numents].origin[1] = (lerp)*tv->entity[i].current.origin[1] + (1-lerp)*tv->entity[i].old.origin[1];
			e->ents[e->numents].origin[2] = (lerp)*tv->entity[i].current.origin[2] + (1-lerp)*tv->entity[i].old.origin[2];
		}

		e->numents++;

		if (e->numents == ENTS_PER_FRAME)
			break;
	}

	SV_EmitPacketEntities(tv, v, e, msg);
}

void UpdateStats(sv_t *qtv, viewer_t *v)
{
	netmsg_t msg;
	char buf[6];
	int i;
	const static unsigned int nullstats[MAX_STATS];

	const unsigned int *stats;

	InitNetMsg(&msg, buf, sizeof(buf));

	if (v->trackplayer < 0)
		stats = nullstats;
	else
		stats = qtv->players[v->trackplayer].stats;

	for (i = 0; i < MAX_STATS; i++)
	{
		if (v->currentstats[i] != stats[i])
		{
			if (stats[i] < 256)
			{
				WriteByte(&msg, svc_updatestat);
				WriteByte(&msg, i);
				WriteByte(&msg, stats[i]);
			}
			else
			{
				WriteByte(&msg, svc_updatestatlong);
				WriteByte(&msg, i);
				WriteLong(&msg, stats[i]);
			}
			SendBufferToViewer(v, msg.data, msg.cursize, true);
			msg.cursize = 0;
			v->currentstats[i] = stats[i];
		}
	}
}

//returns the next prespawn 'buffer' number to use, or -1 if no more
int Prespawn(sv_t *qtv, int curmsgsize, netmsg_t *msg, int bufnum)
{
	int r, ni;
	r = bufnum;

	ni = SendCurrentUserinfos(qtv, curmsgsize, msg, bufnum);
	r += ni - bufnum;
	bufnum = ni;
	bufnum -= MAX_CLIENTS;

	ni = SendCurrentBaselines(qtv, curmsgsize, msg, bufnum);
	r += ni - bufnum;
	bufnum = ni;
	bufnum -= MAX_ENTITIES;

	ni = SendCurrentLightmaps(qtv, curmsgsize, msg, bufnum);
	r += ni - bufnum;
	bufnum = ni;
	bufnum -= MAX_LIGHTSTYLES;

	ni = SendStaticSounds(qtv, curmsgsize, msg, bufnum);
	r += ni - bufnum;
	bufnum = ni;
	bufnum -= MAX_STATICSOUNDS;

	ni = SendStaticEntities(qtv, curmsgsize, msg, bufnum);
	r += ni - bufnum;
	bufnum = ni;
	bufnum -= MAX_STATICENTITIES;

	if (bufnum == 0)
		return -1;

	return r;
}

#define M_PI 3.1415926535897932384626433832795
#include <math.h>
void AngleVectors (short angles[3], float *forward, float *right, float *up)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[1] * (M_PI*2 / 65535);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[0] * (M_PI*2 / 65535);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[2] * (M_PI*2 / 65535);
	sr = sin(angle);
	cr = cos(angle);

	forward[0] = cp*cy;
	forward[1] = cp*sy;
	forward[2] = -sp;
	right[0] = (-1*sr*sp*cy+-1*cr*-sy);
	right[1] = (-1*sr*sp*sy+-1*cr*cy);
	right[2] = -1*sr*cp;
	up[0] = (cr*sp*cy+-sr*-sy);
	up[1] = (cr*sp*sy+-sr*cy);
	up[2] = cr*cp;
}

void PMove(viewer_t *v, usercmd_t *cmd)
{
	int i;
	float fwd[3], rgt[3], up[3];
	AngleVectors(cmd->angles, fwd, rgt, up);

	for (i = 0; i < 3; i++)
		v->origin[i] += (cmd->forwardmove*fwd[i] + cmd->sidemove*rgt[i] + cmd->upmove*up[i])*(cmd->msec/1000.0f);
}

void QTV_Say(sv_t *qtv, viewer_t *v, char *message)
{
	char buf[1024];
	netmsg_t msg;

	message[strlen(message)-1] = '\0';

	InitNetMsg(&msg, buf, sizeof(buf));

	WriteByte(&msg, svc_print);
	WriteByte(&msg, 3);	//PRINT_CHAT
	WriteString2(&msg, v->name);
	WriteString2(&msg, "\x8d ");
	WriteString2(&msg, message+5);
	WriteString(&msg, "\n");

	Multicast(qtv, msg.data, msg.cursize, dem_all, (unsigned int)-1);
}

void ParseQWC(sv_t *qtv, viewer_t *v, netmsg_t *m)
{
	usercmd_t	oldest, oldcmd, newcmd;
	char buf[1024];
	netmsg_t msg;

	v->delta_frame = -1;

	while (m->readpos < m->cursize)
	{
		switch (ReadByte(m))
		{
		case clc_nop:
			return;
		case clc_delta:
			v->delta_frame = ReadByte(m);
			break;
		case clc_stringcmd:	
			ReadString (m, buf, sizeof(buf));
//			printf("stringcmd: %s\n", buf);

			if (!strcmp(buf, "new"))
				SendServerData(qtv, v);
			else if (!strncmp(buf, "say \"", 5) && !qtv->notalking)
				QTV_Say(qtv, v, buf);
			else if (!strncmp(buf, "modellist ", 10))
			{
				char *cmd = buf+10;
				int svcount = atoi(cmd);
				int first;

				while((*cmd >= '0' && *cmd <= '9') || *cmd == '-')
					cmd++;
				first = atoi(cmd);

				InitNetMsg(&msg, buf, sizeof(buf));

				if (svcount != qtv->servercount)
				{	//looks like we changed map without them.
					SendServerData(qtv, v);
					return;
				}

				SendList(qtv, first, qtv->modellist, svc_modellist, &msg);
				SendBufferToViewer(v, msg.data, msg.cursize, true);
			}
			else if (!strncmp(buf, "soundlist ", 10))
			{
				char *cmd = buf+10;
				int svcount = atoi(cmd);
				int first;

				while((*cmd >= '0' && *cmd <= '9') || *cmd == '-')
					cmd++;
				first = atoi(cmd);

				InitNetMsg(&msg, buf, sizeof(buf));

				if (svcount != qtv->servercount)
				{	//looks like we changed map without them.
					SendServerData(qtv, v);
					return;
				}

				SendList(qtv, first, qtv->soundlist, svc_soundlist, &msg);
				SendBufferToViewer(v, msg.data, msg.cursize, true);
			}
			else if (!strncmp(buf, "prespawn", 8))
			{
				char skin[128];

				if (atoi(buf + 9) != qtv->servercount)
					SendServerData(qtv, v);	//we're old.
				else
				{
					int r;
					char *s;
					s = buf+9;
					while((*s >= '0' && *s <= '9') || *s == '-')
						s++;
					while(*s == ' ')
						s++;
					r = atoi(s);

					InitNetMsg(&msg, buf, sizeof(buf));

					r = Prespawn(qtv, v->netchan.message.cursize, &msg, r);
					SendBufferToViewer(v, msg.data, msg.cursize, true);

					if (r < 0)
						sprintf(skin, "%ccmd spawn\n", svc_stufftext);
					else
						sprintf(skin, "%ccmd prespawn %i %i\n", svc_stufftext, qtv->servercount, r);

					SendBufferToViewer(v, skin, strlen(skin)+1, true);
				}
			}
			else if (!strncmp(buf, "spawn", 5))
			{
				char skin[64];
				sprintf(skin, "%cskins\n", svc_stufftext);
				SendBufferToViewer(v, skin, strlen(skin)+1, true);
			}
			else if (!strncmp(buf, "begin", 5))
			{
				if (atoi(buf+6) != qtv->servercount)
					SendServerData(qtv, v);	//this is unfortunate!
				else
					v->thinksitsconnected = true;
			}
			else if (!strncmp(buf, "download", 8))
			{
				netmsg_t m;
				InitNetMsg(&m, buf, sizeof(buf));
				WriteByte(&m, svc_download);
				WriteShort(&m, -1);
				WriteByte(&m, 0);
				SendBufferToViewer(v, m.data, m.cursize, true);
			}
			else if (!strncmp(buf, "drop", 4))
				v->drop = true;
			else if (!strncmp(buf, "ptrack ", 7))
				v->trackplayer = atoi(buf+7);
			else if (!strncmp(buf, "ptrack", 6))
				v->trackplayer = -1;
			else if (!strncmp(buf, "serverinfo", 5))
			{
				char *key, *value, *end;
				int len;
				netmsg_t m;
				InitNetMsg(&m, buf, sizeof(buf));
				WriteByte(&m, svc_print);
				WriteByte(&m, 2);
				end = qtv->serverinfo;
				for(;;)
				{
					if (!*end)
						break;
					key = end;
					value = strchr(key+1, '\\');
					if (!value)
						break;
					end = strchr(value+1, '\\');
					if (!end)
						end = value+strlen(value);

					len = value-key;

					key++;
					while(*key != '\\' && *key)
						WriteByte(&m, *key++);

					for (; len < 20; len++)
						WriteByte(&m, ' ');

					value++;
					while(*value != '\\' && *value)
						WriteByte(&m, *value++);
					WriteByte(&m, '\n');
				}
				WriteByte(&m, 0);

//				WriteString(&m, qtv->serverinfo);
				SendBufferToViewer(v, m.data, m.cursize, true);
			}
			else
			{
				printf("Client sent unknown string command: %s\n", buf);
			}
			
			break;

		case clc_move:
			ReadByte(m);
			ReadByte(m);
			ReadDeltaUsercmd(m, &nullcmd, &oldest);
			ReadDeltaUsercmd(m, &oldest, &oldcmd);
			ReadDeltaUsercmd(m, &oldcmd, &newcmd);
			PMove(v, &newcmd);
			break;
		case clc_tmove:
			v->origin[0] = ((signed short)ReadShort(m))/8.0f;
			v->origin[1] = ((signed short)ReadShort(m))/8.0f;
			v->origin[2] = ((signed short)ReadShort(m))/8.0f;
			break;

		case clc_upload:
			v->drop = true;
			return;

		default:
			v->drop = true;
			return;
		}
	}
}

const char dropcmd[] = {svc_stufftext, 'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', '\n', '\0'};
void QW_UpdateUDPStuff(sv_t *qtv)
{
	char buffer[MAX_MSGLEN*2];
	netadr_t from;
	int fromsize = sizeof(from);
	int read;
	int qport;
	netmsg_t m;

	viewer_t *v, *f;

	if (*qtv->master && (qtv->curtime > qtv->mastersendtime || qtv->mastersendtime > qtv->curtime + 4*1000*60))	//urm... time wrapped?
	{
		if (NET_StringToAddr(qtv->master, &from))
		{
			sprintf(buffer, "a\n%i\n0\n", qtv->mastersequence++);	//fill buffer with a heartbeat
//why is there no \xff\xff\xff\xff ?..
			NET_SendPacket(qtv->qwdsocket, 1, "k", from);	//ping, just like qw.
			NET_SendPacket(qtv->qwdsocket, strlen(buffer), buffer, from);	//ping, just like qw.
		}
		else
			printf("Cannot resolve master %s\n", qtv->master);

		qtv->mastersendtime = qtv->curtime + 3*1000*60;	//3 minuites.
	}

	m.data = buffer;
	m.cursize = 0;
	m.maxsize = MAX_MSGLEN;
	m.readpos = 0;

	for (;;)
	{
		read = recvfrom(qtv->qwdsocket, buffer, sizeof(buffer), 0, (struct sockaddr*)from, &fromsize);

		if (read <= 5)	//otherwise it's a runt or bad.
		{
			if (read < 0)	//it's bad.
				break;
			continue;
		}

		m.data = buffer;
		m.cursize = read;
		m.maxsize = MAX_MSGLEN;
		m.readpos = 0;

		if (*(int*)buffer == -1)
		{	//connectionless message
			ConnectionlessPacket(qtv, &from, &m);
			continue;
		}

		//read the qport
		ReadLong(&m);
		ReadLong(&m);
		qport = ReadShort(&m);

		for (v = qtv->viewers; v; v = v->next)
		{
			if (Net_CompareAddress(&v->netchan.remote_address, &from, v->netchan.qport, qport))
			{
				if (Netchan_Process(&v->netchan, &m))
				{
					v->netchan.outgoing_sequence = v->netchan.incoming_sequence;	//compensate for client->server packetloss.
					if (!v->chokeme || !qtv->chokeonnotupdated)
					{
						v->maysend = true;
						v->chokeme = qtv->chokeonnotupdated;
					}

					ParseQWC(qtv, v, &m);
				}
				break;
			}
		}
	}


	if (qtv->viewers && qtv->viewers->drop)
	{
		printf("Dropping client\n");
		f = qtv->viewers;
		qtv->viewers = f->next;
		Netchan_Transmit(&f->netchan, strlen(dropcmd)+1, dropcmd);
		Netchan_Transmit(&f->netchan, strlen(dropcmd)+1, dropcmd);
		Netchan_Transmit(&f->netchan, strlen(dropcmd)+1, dropcmd);
		free(f);
	}

	for (v = qtv->viewers; v; v = v->next)
	{
		if (v->next && v->next->drop)
		{	//free the next/
			printf("Dropping client\n");
			f = v->next;
			v->next = f->next;

			Netchan_Transmit(&f->netchan, strlen(dropcmd)+1, dropcmd);
			Netchan_Transmit(&f->netchan, strlen(dropcmd)+1, dropcmd);
			Netchan_Transmit(&f->netchan, strlen(dropcmd)+1, dropcmd);
			free(f);
		}

		if (v->maysend && !qtv->parsingconnectiondata)	//don't send incompleate connection data.
		{
			v->maysend = false;
			m.cursize = 0;
			if (v->thinksitsconnected)
			{
				SendPlayerStates(qtv, v, &m);
				UpdateStats(qtv, v);
			}
			Netchan_Transmit(&v->netchan, m.cursize, m.data);

			if (!v->netchan.message.cursize && v->backbuffered)
			{//shift the backbuffers around
				memcpy(v->netchan.message.data, v->backbuf[0].data,  v->backbuf[0].cursize);
				v->netchan.message.cursize = v->backbuf[0].cursize;
				for (read = 0; read < v->backbuffered; read++)
				{
					if (read == v->backbuffered-1)
					{
						v->backbuf[read].cursize = 0;
					}
					else
					{
						memcpy(v->backbuf[read].data, v->backbuf[read+1].data,  v->backbuf[read+1].cursize);
						v->backbuf[read].cursize = v->backbuf[read+1].cursize;
					}
				}
			}
		}
	}
}
