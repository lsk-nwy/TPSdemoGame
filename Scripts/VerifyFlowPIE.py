"""Run manually in PIE on the frontend main menu. Changes only the temporary play world."""
import unreal, time, traceback
S=unreal.CandyFightFlowScreen
O=unreal.CandyFightRunOutcome
def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def game():
    w=world()
    return unreal.GameplayStatics.get_game_instance(w) if w else None
def screen(value):
    g=game()
    return g is not None and g.screen==value
def button(name):
    f=unreal.WidgetLibrary.get_all_widgets_of_class(world(),unreal.CandyFightFlowWidget,True)[0]
    b=next(x for x in unreal.ObjectIterator(unreal.Button) if x.get_name()==name and x.get_path_name().startswith(f.get_path_name()+'.'))
    b.on_clicked.broadcast()
def passed(message):
    unreal.log('[FlowPIE] PASS '+message)
def test_flow():
    yield lambda:screen(S.MAIN_MENU)
    button('FlowPrimary')
    assert screen(S.PREPARATION)
    for name,target in [('Backpack',S.BACKPACK),('Warehouse',S.WAREHOUSE),('Shop',S.SHOP),('Equipment',S.EQUIPMENT)]:
        button('Flow'+name)
        assert screen(target)
        button('FlowPrimary')
        assert screen(S.PREPARATION)
    passed('main menu and four preparation panels')
    for outcome in [O.ESCAPED,O.DEFEATED,O.TIMED_OUT,O.ABANDONED]:
        button('FlowPrimary')
        yield lambda:screen(S.BATTLE)
        w=world(); g=game(); pc=unreal.GameplayStatics.get_player_controller(w,0); p=unreal.GameplayStatics.get_player_pawn(w,0); wallet=pc.player_state
        assert 'L_CandyFight_AI_Test' in w.get_name()
        assert wallet.gold==0 and not wallet.get_editor_property('bHasEscaped')
        assert g.last_result.outcome==O.NONE and g.is_battle_active()
        assert not pc.show_mouse_cursor
        passed('fresh round and hidden crosshair cursor: '+str(outcome))
        wallet.add_gold(123)
        if outcome==O.DEFEATED:
            enemy=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CandyFightEnemyCharacter)[0]
            applied=unreal.GameplayStatics.apply_damage(p,p.health+1.0,enemy.get_controller(),enemy,unreal.DamageType)
            assert applied>0 and p.health==0
        elif outcome==O.ESCAPED:
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CandyFightEnemyCharacter): enemy.destroy_actor()
            gate=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CandyFightEscapeGate)[0]
            pos=gate.get_actor_location(); pos.z+=p.capsule_component.get_scaled_capsule_half_height()
            p.character_movement.set_component_tick_enabled(False)
            p.set_actor_location(pos,False,True)
            passed('awaiting real gate countdown')
        elif outcome==O.TIMED_OUT:
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CandyFightEnemyCharacter): enemy.destroy_actor()
            unreal.GameplayStatics.set_global_time_dilation(w,20.0)
            passed('awaiting actual GameMode timeout with accelerated PIE clock')
        else:
            g.toggle_battle_menu()
            assert screen(S.PAUSE) and unreal.GameplayStatics.is_game_paused(w)
            button('FlowPrimary')
            assert screen(S.BATTLE) and not unreal.GameplayStatics.is_game_paused(w)
            assert not pc.show_mouse_cursor
            g.toggle_battle_menu()
            button('FlowSecondary')
        yield lambda:screen(S.SETTLEMENT)
        w=world(); g=game()
        assert g.last_result.outcome==outcome
        assert g.last_result.gold==123 and g.last_result.elapsed_seconds>=0
        assert not g.is_battle_active() and unreal.GameplayStatics.is_game_paused(w)
        assert not g.finish_battle(O.ABANDONED)
        if outcome==O.ESCAPED: assert wallet.get_editor_property('bHasEscaped')
        if outcome==O.TIMED_OUT: unreal.GameplayStatics.set_global_time_dilation(w,1.0)
        passed('actual terminal event and settlement: '+str(outcome)+' Gold='+str(g.last_result.gold))
        if outcome==O.DEFEATED:
            button('FlowSecondary')
            yield lambda:screen(S.MAIN_MENU) and 'Frontend' in world().get_name()
            button('FlowPrimary')
        else:
            button('FlowPrimary')
            yield lambda:screen(S.PREPARATION) and 'Frontend' in world().get_name()
        assert screen(S.PREPARATION) and not unreal.GameplayStatics.is_game_paused(world())
        passed('returned to frontend: '+str(outcome))
    passed('ALL COMPLETE: four real endings, repeated travel, fresh round gold, UI button delegates')
if world() is None or 'UEDPIE_' not in world().get_path_name() or not screen(S.MAIN_MENU):
    raise RuntimeError('Start PIE on L_CandyFight_Frontend and remain on the main menu first')
state={'iterator':test_flow(),'condition':None,'deadline':time.monotonic()+45,'handle':None}
def tick(dt):
    try:
        if time.monotonic()>state['deadline']: raise AssertionError('Timed out waiting for next flow state')
        if state['condition'] is not None and not state['condition'](): return
        state['condition']=next(state['iterator'])
        state['deadline']=time.monotonic()+45
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(state['handle'])
    except Exception:
        unreal.log_error('[FlowPIE] FAILED '+traceback.format_exc())
        if world(): unreal.GameplayStatics.set_global_time_dilation(world(),1.0)
        unreal.unregister_slate_post_tick_callback(state['handle'])
state['handle']=unreal.register_slate_post_tick_callback(tick)
print('Scheduled FlowPIE checks')
