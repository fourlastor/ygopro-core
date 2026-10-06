project('ocgcore', 'cpp',
	license : 'AGPL-3.0-or-later',
	default_options : ['cpp_std=c++17'],
	version : '9.1'
)

lua_dep = dependency('lua-5.4')
threads_dep = dependency('threads')

args = []
link_args = []
if get_option('default_library') == 'shared'
	args += ['-DOCGCORE_EXPORT_FUNCTIONS', '-fvisibility=hidden']
	# Keep core-internal allocation calls inside this ELF DSO.  dlopen hosts
	# otherwise preempt the arena-aware global new/delete hooks with libstdc++.
	if host_machine.system() == 'linux' or host_machine.system() == 'freebsd'
		link_args += ['-Wl,-Bsymbolic-functions']
	endif
endif

ocgcore_src = files([
	'card.cpp',
	'duel.cpp',
	'duel_arena.cpp',
	'duel_swap.cpp',
	'effect.cpp',
	'field.cpp',
	'interpreter.cpp',
	'libcard.cpp',
	'libdebug.cpp',
	'libduel.cpp',
	'libeffect.cpp',
	'libgroup.cpp',
	'ocgapi.cpp',
	'operations.cpp',
	'playerop.cpp',
	'processor.cpp',
	'processor_visit.cpp',
	'scriptlib.cpp',
])

library('ocgcore', ocgcore_src, cpp_args : args, link_args : link_args, dependencies : [lua_dep, threads_dep])
