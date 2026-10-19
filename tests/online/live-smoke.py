#!/usr/bin/env python3
"""Opt-in real Wikisource smoke; never registered in CTest."""
import json
import selectors
import subprocess
import sys

engine = sys.argv[1] if len(sys.argv) > 1 else 'build-online-engine/reader-online'
p = subprocess.Popen([engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
selector = selectors.DefaultSelector()
selector.register(p.stdout, selectors.EVENT_READ)
source = dict(id='wikisource-zh', kind='mediawiki', baseUrl='https://zh.wikisource.org')
sequence = 0

def call(op, **fields):
    global sequence
    sequence += 1
    request = dict(protocol=1, id=str(sequence), session='live', op=op, source=source, **fields)
    p.stdin.write(json.dumps(request, ensure_ascii=False) + '\n')
    p.stdin.flush()
    if not selector.select(90):
        raise RuntimeError('Engine response deadline')
    reply = json.loads(p.stdout.readline())
    if not reply.get('ok'):
        raise RuntimeError(reply.get('error'))
    return reply['result']

try:
    for keyword in ['红楼梦', '三国演义']:
        results = call('search', keyword=keyword, page=1)['books']
        assert results, 'Search returned no works'
        chosen = results[0]
        print('SEARCH', keyword, chosen, flush=True)
        toc = call('toc', url=chosen['url'])
        print('TOC', len(toc['chapters']), 'CHOICES', len(toc['choices']), flush=True)
        for _ in range(3):
            if toc['chapters'] or not toc['choices']:
                break
            chosen = toc['choices'][0]
            print('SELECT', chosen, flush=True)
            toc = call('toc', url=chosen['url'])
        assert toc['chapters'], 'No chapter links found'
        chapter = call('chapter', url=toc['chapters'][0]['url'])
        assert len(chapter['text']) > 100, 'Chapter content too short'
        print('CHAPTER', chapter['url'], len(chapter['text']), chapter['text'][:80], flush=True)
finally:
    p.stdin.close()
    p.wait(timeout=5)
