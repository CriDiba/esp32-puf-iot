import { createHash, randomBytes } from 'crypto'

export function getRandomChallenge(): string {
  const buf = randomBytes(8)
  return buf.toString('hex')
}

export function calculateResponse(puf: string, challenge: string): string {
  const pufBuf = Buffer.from(puf, 'hex')
  const challBuf = Buffer.from(challenge, 'hex')

  const bufToHash = Buffer.concat([challBuf, pufBuf])

  const hash = createHash('sha256')
  hash.update(bufToHash)

  return hash.digest('hex')
}

export function calculateResponseV0(puf: string, challenge: string): string {
  const pufBuf = Buffer.from(puf, 'hex')
  const challBuf = Buffer.from(challenge, 'hex')

  const xoredBuf = Buffer.from(pufBuf.map((b, i) => b ^ challBuf[i]))
  return xoredBuf.toString('hex')
}
