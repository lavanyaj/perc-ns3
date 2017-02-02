## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
def build(bld):
    obj = bld.create_ns3_program('exp1', ['core', 'internet' , 'network' , 'applications' , 'point-to-point', 'topology-read'])
    obj.source = ['exp1.cc', 'init_all.cc', 'common_utils.cc', 'sending_app.cc']
