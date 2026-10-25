// Restore authenticated missing characters after native _Prebuilt. Its strict
// placement check can skip an entry silently; a terminal living snapshot is a
// carried placement, not a new deployment. Never turn that survivor into a
// downed timer. Downed entries still use the original card timer/DummyManager.
std::atomic<unsigned> nmh_downed_restores{0};
std::atomic<unsigned> nmh_living_restores{0};
cnjson::Value nmh_absent_last;
int32_t nmh_enum_field(void* object,const char* name,const char* type){
    auto field=profile_field(api.il2cpp_object_get_class(object),name);
    if(!matches_type(api.il2cpp_field_get_type(field),type))throw std::runtime_error("multiplayer respawn field declaration differs");
    int32_t value=0;api.il2cpp_field_get_value(object,field,&value);return value;
}
void nmh_complete_downed(void* task,void* controller,void* help,uint64_t serial,TemporaryRoots& roots){
    std::map<NMHKey,NMHCarryTarget> pending;
    {std::lock_guard<std::mutex> lock(nmh_mutex);
        if(nmh_carry_failed||serial!=nmh_carry_serial)return;
        for(const auto& row:nmh_carry_targets)if(!row.second.token&&!nmh_carry_applied.count(row.first))pending.insert(row);
    }
    if(pending.empty())return;
    auto context=nb_object();auto recovered=nb_array();
    try{
        auto model=roots.retain(field_object(nb_centre(),"battleData"));
        auto chess=roots.retain(field_object(model,"battleChessInsts"));
        auto map=roots.retain(invoke(nmh_method(0x600fbe8),controller));
        auto dummy=roots.retain(field_object(nb_manager(),"dummyManager"));
        if(!chess||!map||!dummy)throw std::runtime_error("multiplayer downed carry has no native board/dummy manager");
        auto type=api.il2cpp_object_get_class(chess);auto count=nmh_result<int32_t>(invoke(method(type,"get_Count",0),chess));
        if(count<0||count>4096)throw std::runtime_error("multiplayer downed carry native board exceeds bound");
        for(const auto& row:pending){const auto& key=row.first;const auto& target=row.second;int32_t inst=key[1];
            context.object["respawnInstId"]=nb_integer(inst);
            context.object["retreat"]=nb_boolean(target.retreat);
            context.object["seat"]=nb_integer(key[0]);
            context.object["round"]=nb_integer(nmh_round);
            {std::lock_guard<std::mutex> lock(nmh_mutex);
                if(nmh_carry_failed||serial!=nmh_carry_serial||nmh_phase!="help"||hold_get(nmh_carry_task_root)!=task||hold_get(nmh_carry_controller_root)!=controller)
                    throw std::runtime_error("multiplayer downed carry lost current task/plan");}
            nmh_check_operator_status(help,inst,target,roots);
            auto owners=field_object(field_object(nb_centre(),"indexer"),"instId2PlayerIndex");
            if(nmh_dictionary<int32_t>(owners,inst)!=key[0])throw std::runtime_error("multiplayer downed carry owner differs");
            void* selected=nullptr;
            for(int32_t i=0;i<count;++i){void* args[]={&i};auto item=roots.retain(invoke(method(type,"get_Item",1),chess,args));
                if(item&&nmh_int(item,"instId")==inst&&nmh_enum_field(item,"chessType","Torappu.AutoChessItemType")==0){
                    if(selected)throw std::runtime_error("multiplayer downed carry native chess is ambiguous");selected=item;}}
            if(!selected||nmh_int(selected,"playerIndex")!=key[0])throw std::runtime_error("multiplayer downed carry missing native chess");
            struct Grid {int32_t row,col;} grid{};
            auto posField=profile_field(api.il2cpp_object_get_class(selected),"pos");
            if(!matches_type(api.il2cpp_field_get_type(posField),"Torappu.GridPosition"))throw std::runtime_error("multiplayer downed carry grid declaration differs");
            api.il2cpp_field_get_value(selected,posField,&grid);
            if(grid.row<0||grid.row>32767||grid.col<0||grid.col>65535)throw std::runtime_error("multiplayer downed carry grid invalid");
            int32_t direction=nmh_enum_field(selected,"dir","Torappu.SharedConsts+Direction");
            auto position=nb_object();position.object["instId"]=nb_integer(inst);position.object["chessType"]=nb_integer(0);
            position.object["position"]=nb_integer((uint32_t(grid.row)<<16)|uint32_t(grid.col));position.object["direction"]=nb_integer(direction);
            auto canonical=build_value(api.il2cpp_class_get_type(cls("Torappu.Battle.AutoChess.ChessPositionInfo")),position,roots).object;
            void* canonicalArgs[]={canonical};invoke(nmh_method(0x6013860),field_object(nb_manager(),"mapAreaManager"),canonicalArgs);
            // ChessPositionInfo.direction keeps its native Direction enum declaration
            // after coordinate conversion; it is not a System.Int32 field.
            if(nmh_int(canonical,"position")!=target.position||
               nmh_enum_field(canonical,"direction","Torappu.SharedConsts+Direction")!=target.direction)
                throw std::runtime_error("multiplayer downed carry native placement differs from plan");
            void* tileArgs[]={&grid};auto tile=roots.retain(invoke(nmh_method(0x60111be),map,tileArgs));
            uint8_t token=0;uint32_t uid=0;void* uidArgs[]={&inst,&token,&uid};
            if(!tile||!nmh_result<uint8_t>(invoke(nmh_method(0x6013525),nullptr,uidArgs))||!uid)throw std::runtime_error("multiplayer downed carry card/tile missing");
            void* cardArgs[]={&uid};auto card=roots.retain(invoke(nmh_method(0x6013523),nullptr,cardArgs));
            if(!card||api.il2cpp_object_get_class(card)!=cls("Torappu.Battle.Deck+CharacterCard")||nmh_result<int32_t>(invoke(nmh_method(0x601020c),card))!=1)
                throw std::runtime_error("multiplayer downed carry requires native single-character card policy");
            auto data=roots.retain(invoke(nmh_method(0x6010123),card));
            if(nmh_uint(data,"uniqueId")!=uid||nmh_result<int32_t>(invoke(nmh_method(0x6010127),card))!=nmh_enum_field(selected,"side","Torappu.PlayerSide"))
                throw std::runtime_error("multiplayer downed carry card identity differs");
            auto units=roots.retain(invoke(nmh_method(0x600fbf7),controller));
            auto live=roots.retain(invoke(nmh_method(0x6012fe9),units,cardArgs));
            auto occupant=roots.retain(invoke(nmh_method(0x601143a),tile));
            if((live&&!nmh_result<uint8_t>(invoke(nmh_method(0x6012f2b),live)))||
               (occupant&&!nmh_result<uint8_t>(invoke(nmh_method(0x6012f2b),occupant))))
                throw std::runtime_error("multiplayer downed carry still has a living character/occupied tile");
            int32_t state=nmh_result<int32_t>(invoke(nmh_method(0x6010113),card));
            context.object["respawnCardState"]=nb_integer(state);
            // READY (fresh board) or RESPAWNING (already downed). USING is
            // not silently converted: it may belong to a living instance.
            if(state!=1&&state!=3)throw std::runtime_error("multiplayer downed carry card is still in use");
            void* instArgs[]={&inst};auto priorTile=roots.retain(invoke(nmh_method(0x60137d4),dummy,instArgs));
            if(priorTile&&priorTile!=tile)throw std::runtime_error("multiplayer downed carry dummy belongs to an old tile");
            void* dummyArgs[]={tile};auto body=roots.retain(invoke(nmh_method(0x60137d0),dummy,dummyArgs));
            if(body){int32_t actual=-1;void* idArgs[]={body,&actual};
                if(!nmh_result<uint8_t>(invoke(nmh_method(0x6012f2b),body))||!nmh_result<uint8_t>(invoke(nmh_method(0x601351b),nullptr,idArgs))||actual!=inst)
                    throw std::runtime_error("multiplayer downed carry tile has another dummy");
            }
            if(!target.retreat){
                if((occupant&&occupant!=body)||(live&&live!=body))
                    throw std::runtime_error("multiplayer living carry has an unregistered occupied/dummy actor");
                // Already checked the immutable HP/SP input, owner/card UID,
                // converted native position/direction and empty live occupancy.
                // Only this missing planned survivor may restore its placement;
                // no global spawn hook or deployment-rule change is involved.
                uint8_t no=0,yes=1;void* checkArgs[]={tile,&no,&yes,&no};
                uint8_t strict=nmh_result<uint8_t>(invoke(nmh_method(0x6010210),card,checkArgs));
                context.object["nativeBuildable"]=nb_boolean(strict!=0);
                context.object["nativePosition"]=nb_integer((uint32_t(grid.row)<<16)|uint32_t(grid.col));
                int32_t side=nmh_enum_field(selected,"side","Torappu.PlayerSide");void* deckArgs[]={&side};
                auto deck=roots.retain(invoke(nmh_method(0x600fbef),controller,deckArgs));
                if(!deck)throw std::runtime_error("multiplayer living carry deck missing");
                // Same native free prebuilt generation, with placement checks
                // waived only for an authenticated, already-occupied snapshot
                // position. Native prefab/card/buff creation remains original.
                void* spawnArgs[]={&uid,&direction,tile,&strict,&yes,&yes,&no};
                if(!nmh_result<uint8_t>(invoke(nmh_method(0x60100e1),deck,spawnArgs)))
                    throw std::runtime_error("multiplayer living carry original generation refused");
                auto restored=roots.retain(invoke(nmh_method(0x6012fe9),units,cardArgs));
                int32_t actual=-1;void* idArgs[]={restored,&actual};
                if(!restored||!nmh_result<uint8_t>(invoke(nmh_method(0x6010353),restored))||
                   nmh_result<uint8_t>(invoke(nmh_method(0x6012f2b),restored))||
                   !nmh_result<uint8_t>(invoke(nmh_method(0x601351b),nullptr,idArgs))||actual!=inst||
                   nmh_default_origin(restored,roots)!=target.position)
                    throw std::runtime_error("multiplayer living carry generated character differs");
                if(body)invoke(nmh_method(0x60137ef),dummy,dummyArgs);
                nmh_apply_status(task,restored,inst,help,nullptr);
                {std::lock_guard<std::mutex> lock(nmh_mutex);
                    if(nmh_carry_failed||serial!=nmh_carry_serial||!nmh_carry_applied.count(key))
                        throw std::runtime_error("multiplayer living carry original status incomplete");
                    nmh_absent_last=context;}
                ++nmh_living_restores;continue;
            }
            if(!body){void* args[]={data,tile,&direction};body=roots.retain(invoke(nmh_method(0x60137e5),dummy,args));}
            if(!body||!nmh_result<uint8_t>(invoke(nmh_method(0x6012f2b),body))||invoke(nmh_method(0x60137d4),dummy,instArgs)!=tile)
                throw std::runtime_error("multiplayer downed carry native dummy was not registered");
            uint8_t wait=1;void* resetArgs[]={&wait};invoke(nmh_method(0x601014e),card,resetArgs);
            int32_t respawning=3;void* stateArgs[]={&respawning};invoke(nmh_method(0x6010173),card,stateArgs);
            if(nmh_result<int32_t>(invoke(nmh_method(0x6010113),card))!=respawning)throw std::runtime_error("multiplayer downed carry native timer state not applied");
            {std::lock_guard<std::mutex> lock(nmh_mutex);
                if(serial!=nmh_carry_serial||nmh_carry_failed)throw std::runtime_error("multiplayer downed carry plan changed during native reset");
                nmh_carry_applied.insert(key);}
            ++nmh_downed_restores;
            auto proof=nb_object();proof.object["instId"]=nb_integer(inst);proof.object["previousCardState"]=nb_integer(state);
            proof.object["reusedDummy"]=nb_boolean(priorTile!=nullptr);recovered.array.push_back(proof);
        }
    }catch(const std::exception& error){context.object["recoveredDowned"]=recovered;{std::lock_guard<std::mutex> lock(nmh_mutex);nmh_absent_last=context;}nmh_carry_record_failure(error.what(),context);throw;}
}
