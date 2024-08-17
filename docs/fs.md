---
outline: false
footer: false
title: File System
---

# File System

### Kun.chmod

**chmod**(path: string, mode: number): Promise\<void\>

```js
await Kun.chmod('/path/to/file', 0o666);
```

### Kun.chown

**chown**(path: string, uid: number, gid: number): Promise\<void\>

```js
await Kun.chown('/path/to/file', 1000, 1002);
```

### Kun.copyFile

**copyFile**(from: string, to: string): Promise\<void\>

```js
await Kun.copyFile('from.txt', 'to.txt');
```


