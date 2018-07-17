#!/usr/bin/env node
const method = process.argv[2]

process.stdin.setEncoding('utf8')
console.log("Hello")
if (method.toUpperCase() === 'POST') {
  const content_length = parseInt(process.argv[3])
  let length = 0
  process.stdin.on('readable', () => {
    const chunk = process.stdin.read();
    if (chunk != null) {
      length += chunk.length
      console.log(`You Posted: ${chunk}\n`)
    }
    if (length === content_length) {
      process.exit(0)
    }
  })
} else if (method.toUpperCase() === 'GET') {
  const query_string = process.argv[3]
  console.log(`You Queried: ${query_string}\n`)
  process.exit(0)
}
