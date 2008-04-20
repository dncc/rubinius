#include "objects.hpp"
#include "builtin_list.hpp"
#include "vm.hpp"
#include "objectmemory.hpp"

#include <cxxtest/TestSuite.h>

using namespace rubinius;

extern "C" {
  static bool debugger_hit;
  static void debugger_check(int sig) {
    debugger_hit = true;
  }
}


class TestTask : public CxxTest::TestSuite {
  public:

#undef state
  VM* state;

  void setUp() {
    state = new VM();
  }

  void tearDown() {
    delete state;
  }

  void test_create() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    Task* task = Task::create(state, Qnil, cm);

    TS_ASSERT(kind_of<Task>(task));

    TS_ASSERT(kind_of<MethodContext>(task->active));

    TS_ASSERT_EQUALS(0, task->ip);
    TS_ASSERT_EQUALS(-1, task->sp);
  }

  void test_send_message() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    Task* task = Task::create(state, Qnil, cm);

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("blah");
    msg.send_site = SendSite::create(state, state->symbol("blah"));
    msg.args = 0;

    MethodContext* cur = task->active;

    task->send_message(msg);

    TS_ASSERT(cur != task->active);

    MethodContext* ncur = task->active;

    TS_ASSERT_EQUALS(ncur->self, Qtrue);
    TS_ASSERT_EQUALS(ncur->sender, cur);
  }

  void test_send_message_slowly() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    Task* task = Task::create(state, Qnil, cm);

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("blah");
    msg.send_site = (SendSite*)Qnil;
    msg.args = 0;

    MethodContext* cur = task->active;

    task->send_message_slowly(msg);

    TS_ASSERT(cur != task->active);

    MethodContext* ncur = task->active;

    TS_ASSERT_EQUALS(ncur->self, Qtrue);
    TS_ASSERT_EQUALS(ncur->sender, cur);
  }

  void test_send_message_sets_up_fixed_locals() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->required_args = Object::i2n(2);
    cm->total_args = cm->required_args;
    cm->local_count = cm->required_args;
    cm->stack_size =  cm->required_args;
    cm->splat = Qnil;

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Task* task = Task::create(state, Qnil, cm);

    Tuple* input_stack = Tuple::from(state, 2, Object::i2n(3), Object::i2n(4));
    task->stack = input_stack;
    task->sp = 1;

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("blah");
    msg.send_site = SendSite::create(state, state->symbol("blah"));
    msg.use_from_task(task, 2);

    task->send_message(msg);

    TS_ASSERT(task->stack != input_stack);
    TS_ASSERT_EQUALS(task->stack->field_count, 2);
    TS_ASSERT_EQUALS(task->stack->field[0], Object::i2n(3));
    TS_ASSERT_EQUALS(task->stack->field[1], Object::i2n(4));
  }

  void test_send_message_sets_up_fixed_locals_with_optionals() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->required_args = Object::i2n(2);
    cm->total_args = Object::i2n(4);
    cm->local_count = cm->total_args;
    cm->stack_size =  cm->total_args;
    cm->splat = Qnil;

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Task* task = Task::create(state, Qnil, cm);

    Tuple* input_stack = Tuple::from(state, 3,
        Object::i2n(3), Object::i2n(4), Object::i2n(5));
    task->stack = input_stack;
    task->sp = 2;

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("blah");
    msg.send_site = SendSite::create(state, state->symbol("blah"));
    msg.use_from_task(task, 3);

    task->send_message(msg);

    TS_ASSERT(task->stack != input_stack);
    TS_ASSERT_EQUALS(task->stack->field_count, 4);
    TS_ASSERT_EQUALS(task->stack->field[0], Object::i2n(3));
    TS_ASSERT_EQUALS(task->stack->field[1], Object::i2n(4));
    TS_ASSERT_EQUALS(task->stack->field[2], Object::i2n(5));
    TS_ASSERT_EQUALS(task->stack->field[3], Qnil);

    TS_ASSERT(task->passed_arg_p(3));
    TS_ASSERT(!task->passed_arg_p(4));
  }

  void test_send_message_sets_up_fixed_locals_with_splat() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->required_args = Object::i2n(2);
    cm->total_args = cm->required_args;
    cm->local_count = Object::i2n(3);
    cm->stack_size =  cm->local_count;
    cm->splat = Object::i2n(2);

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Task* task = Task::create(state, Qnil, cm);

    Tuple* input_stack = Tuple::from(state, 4,
        Object::i2n(3), Object::i2n(4), Object::i2n(5) ,Object::i2n(6));
    task->stack = input_stack;
    task->sp = 3;

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("blah");
    msg.send_site = SendSite::create(state, state->symbol("blah"));
    msg.use_from_task(task, 4);

    task->send_message(msg);

    TS_ASSERT(task->stack != input_stack);
    TS_ASSERT_EQUALS(task->stack->field_count, 3);
    TS_ASSERT_EQUALS(task->stack->field[0], Object::i2n(3));
    TS_ASSERT_EQUALS(task->stack->field[1], Object::i2n(4));

    Array* splat = as<Array>(task->stack->field[2]);

    TS_ASSERT_EQUALS(splat->size(), 2);
    TS_ASSERT_EQUALS(splat->get(state, 0), Object::i2n(5));
    TS_ASSERT_EQUALS(splat->get(state, 1), Object::i2n(6));
  }

  void test_send_message_sets_up_fixed_locals_with_optional_and_splat() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->required_args = Object::i2n(1);
    cm->total_args = Object::i2n(2);
    cm->local_count = Object::i2n(3);
    cm->stack_size =  cm->local_count;
    cm->splat = Object::i2n(2);

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Task* task = Task::create(state, Qnil, cm);

    Tuple* input_stack = Tuple::from(state, 4,
        Object::i2n(3), Object::i2n(4), Object::i2n(5) ,Object::i2n(6));
    task->stack = input_stack;
    task->sp = 3;

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("blah");
    msg.send_site = SendSite::create(state, state->symbol("blah"));
    msg.use_from_task(task, 4);

    task->send_message(msg);

    TS_ASSERT(task->stack != input_stack);
    TS_ASSERT_EQUALS(task->stack->field_count, 3);
    TS_ASSERT_EQUALS(task->stack->field[0], Object::i2n(3));
    TS_ASSERT_EQUALS(task->stack->field[1], Object::i2n(4));

    Array* splat = as<Array>(task->stack->field[2]);

    TS_ASSERT_EQUALS(splat->size(), 2);
    TS_ASSERT_EQUALS(splat->get(state, 0), Object::i2n(5));
    TS_ASSERT_EQUALS(splat->get(state, 1), Object::i2n(6));
  }

  void test_simple_return() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);

    MethodContext* top = task->active;

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("blah");
    msg.send_site = SendSite::create(state, state->symbol("blah"));
    msg.args = 0;

    Tuple* outer_stack = task->stack;

    task->send_message(msg);
    
    Tuple* inner_stack = task->stack;

    TS_ASSERT(outer_stack != inner_stack);

    task->simple_return(Object::i2n(3));
    TS_ASSERT_EQUALS(task->active, top);
    TS_ASSERT_EQUALS(task->sp, 0);
  }

  void test_locate_method_on() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    Executable* x = task->locate_method_on(Qtrue, state->symbol("blah"), Qfalse);

    TS_ASSERT_EQUALS(x, cm);
  }

  void test_locate_method_on_private() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);

    CompiledMethod::Visibility* vis = CompiledMethod::Visibility::create(state);
    vis->method = cm;
    vis->visibility = G(sym_private);

    G(true_class)->method_table->store(state, state->symbol("blah"), vis);

    Executable* x = task->locate_method_on(Qtrue, state->symbol("blah"), Qfalse);

    TS_ASSERT_EQUALS(x, Qnil);
  }

  void test_locate_method_on_private_private_send() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);

    CompiledMethod::Visibility* vis = CompiledMethod::Visibility::create(state);
    vis->method = cm;
    vis->visibility = G(sym_private);

    G(true_class)->method_table->store(state, state->symbol("blah"), vis);

    Executable* x = task->locate_method_on(Qtrue, state->symbol("blah"), Qtrue);

    TS_ASSERT_EQUALS(x, cm);
  }

  void test_attach_method() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);

    task->attach_method(Qtrue, state->symbol("blah"), cm);

    TS_ASSERT_EQUALS(cm, G(true_class)->method_table->fetch(state, state->symbol("blah")));
  }

  void test_add_method() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);

    task->add_method(G(true_class), state->symbol("blah"), cm);

    TS_ASSERT_EQUALS(cm, G(true_class)->method_table->fetch(state, state->symbol("blah")));
  }

  void test_check_serial() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);

    Task* task = Task::create(state);

    cm->serial = Object::i2n(0);

    G(true_class)->method_table->store(state, state->symbol("blah"), cm);

    TS_ASSERT( task->check_serial(Qtrue, state->symbol("blah"), 0));
    TS_ASSERT(!task->check_serial(Qtrue, state->symbol("blah"), 1));
  }

  void test_const_get_from_specific_module() {
    bool found;
    G(true_class)->set_const(state, "Number", Object::i2n(3));
    Task* task = Task::create(state);

    TS_ASSERT_EQUALS(
        task->const_get(G(true_class), state->symbol("Number"), &found),
        Object::i2n(3));

    TS_ASSERT(found);
  }

  void test_const_get_from_in_superclass() {
    bool found;
    G(object)->set_const(state, "Number", Object::i2n(3));
    Task* task = Task::create(state);

    TS_ASSERT_EQUALS(
        task->const_get(G(true_class), state->symbol("Number"), &found),
        Object::i2n(3));

    TS_ASSERT(found);
  }

  void test_const_get_from_module_in_object_are_not_found() {
    bool found;
    G(object)->set_const(state, "Number", Object::i2n(3));
    Task* task = Task::create(state);

    Module* mod = state->new_module("Test");

    TS_ASSERT_EQUALS(
        task->const_get(mod, state->symbol("Number"), &found),
        Qnil);

    TS_ASSERT(!found);
  }

  void test_const_get_in_context() {
    bool found;
    Module* parent = state->new_module("Parent");
    Module* child =  state->new_module("Parent::Child");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    StaticScope* cs = StaticScope::create(state);
    SET(cs, module, child);
    SET(cs, parent, ps);

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, cs);

    parent->set_const(state, "Number", Object::i2n(3));
    child->set_const(state, "Name", state->symbol("blah"));

    TS_ASSERT_EQUALS(
        task->const_get(state->symbol("Number"), &found),
        Object::i2n(3));

    TS_ASSERT(found);

    TS_ASSERT_EQUALS(
        task->const_get(state->symbol("Name"), &found),
        state->symbol("blah"));

    TS_ASSERT(found);
  }

  void test_const_get_in_context_uses_superclass_too() {
    bool found;
    Module* parent = state->new_module("Parent");
    Module* child =  state->new_module("Parent::Child");
    Module* inc =    state->new_module("Included");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    StaticScope* cs = StaticScope::create(state);
    SET(cs, module, child);
    SET(cs, parent, ps);

    inc->set_const(state, "Age", Object::i2n(28));

    SET(child, superclass, inc);

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, cs);

    TS_ASSERT_EQUALS(
      task->const_get(state->symbol("Age"), &found),
      Object::i2n(28));

    TS_ASSERT(found);
  }

  void test_const_get_in_context_checks_Object() {
    bool found;
    Module* parent = state->new_module("Parent");
    Module* child =  state->new_module("Parent::Child");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    StaticScope* cs = StaticScope::create(state);
    SET(cs, module, child);
    SET(cs, parent, ps);

    G(object)->set_const(state, "Age", Object::i2n(28));

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, cs);

    TS_ASSERT_EQUALS(
      task->const_get(state->symbol("Age"), &found),
      Object::i2n(28));

    TS_ASSERT(found);
  }

  void test_const_set() {
    Module* parent = state->new_module("Parent");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, ps);

    task->const_set(parent, state->symbol("Age"), Object::i2n(28));

    TS_ASSERT_EQUALS(parent->get_const(state, "Age"), Object::i2n(28));
  }

  void test_const_set_under_scope() {
    Module* parent = state->new_module("Parent");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, ps);

    task->const_set(state->symbol("Age"), Object::i2n(28));

    TS_ASSERT_EQUALS(parent->get_const(state, "Age"), Object::i2n(28));
  }

  void test_yield_debugger() {
    signal(SIGEMT, debugger_check);
    Task* task = Task::create(state);

    task->yield_debugger(Qtrue);

    TS_ASSERT(debugger_hit);
  }

  void test_current_module() {
    Module* parent = state->new_module("Parent");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, ps);

    TS_ASSERT_EQUALS(task->current_module(), parent);
  }

  void test_open_class() {
    Module* parent = state->new_module("Parent");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, ps);

    bool created;
    Class* cls = task->open_class(Qnil, state->symbol("Person"), &created);
    TS_ASSERT(created);
    TS_ASSERT(kind_of<Class>(cls));

    TS_ASSERT_EQUALS(cls->name, state->symbol("Parent::Person"));
    TS_ASSERT_EQUALS(parent->get_const(state, "Person"), cls);
  }

  void test_open_class_under_specific_module() {
    Module* parent = state->new_module("Parent");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, ps);

    bool created;
    Class* cls = task->open_class(G(object), Qnil, state->symbol("Person"), &created);
    TS_ASSERT(created);
    TS_ASSERT(kind_of<Class>(cls));

    TS_ASSERT_EQUALS(cls->name, state->symbol("Person"));
    TS_ASSERT_EQUALS(G(object)->get_const(state, "Person"), cls);
  }

  void test_open_module() {
    Module* parent = state->new_module("Parent");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, ps);

    Module* mod = task->open_module(state->symbol("Person"));
    TS_ASSERT(kind_of<Module>(mod));

    TS_ASSERT_EQUALS(mod->name, state->symbol("Parent::Person"));
    TS_ASSERT_EQUALS(parent->get_const(state, "Person"), mod);
  }

  void test_open_module_under_specific_module() {
    Module* parent = state->new_module("Parent");

    StaticScope* ps = StaticScope::create(state);
    SET(ps, module, parent);
    ps->parent = (StaticScope*)Qnil;

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq = ISeq::create(state, 40);
    cm->stack_size = Object::i2n(1);

    Task* task = Task::create(state, Qnil, cm);
    SET(cm, scope, ps);

    Module* mod = task->open_module(G(object), state->symbol("Person"));
    TS_ASSERT(kind_of<Module>(mod));

    TS_ASSERT_EQUALS(mod->name, state->symbol("Person"));
    TS_ASSERT_EQUALS(G(object)->get_const(state, "Person"), mod);
  }
};
