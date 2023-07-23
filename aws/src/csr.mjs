import { X509, KEYUTIL, KJUR } from "jsrsasign";
import { GetObjectCommand, S3Client } from "@aws-sdk/client-s3";
import { CA_CRED_PATH } from "./config.mjs";
import { randomBytes } from "crypto";

export async function verifyCSR(csrPem) {
  // verify csr signature
  if (KJUR.asn1.csr.CSRUtil.verifySignature(csrPem)) {
    console.log("Certification request (CSR) verified.");
  } else {
    throw new Error("Signature not verified.");
  }

  // create certification request object
  const csr = KJUR.asn1.csr.CSRUtil.getParam(csrPem);

  // load ca keys
  if (!CA_CRED_PATH) {
    throw new Error("CA credentials path not exists");
  }

  const CACredentialsPath = CA_CRED_PATH.split(":::")[1];
  const CACredentialsBucket = CACredentialsPath.substring(
    0,
    CACredentialsPath.indexOf("/")
  );
  const CACredentialsKey = CACredentialsPath.substring(
    CACredentialsPath.indexOf("/") + 1
  );

  const s3client = new S3Client({ region: "eu-west-1" });
  const getCACred = new GetObjectCommand({
    Bucket: CACredentialsBucket,
    Key: CACredentialsKey,
  });

  const caCredObj = await s3client.send(getCACred);
  if (!caCredObj.Body) {
    throw new Error("CA credentials not found");
  }

  const caCredStr = Buffer.from(
    await caCredObj.Body?.transformToByteArray()
  ).toString();

  const caCredentials = JSON.parse(caCredStr);
  const { caCertPem, caKeyPem } = caCredentials;

  // load ca certificate
  const caCert = new X509();
  caCert.readCertPEM(caCertPem);

  // load ca key
  const caKey = KEYUTIL.getKey(caKeyPem);

  console.log("Creating certificate...");

  const time = new KJUR.asn1.DERUTCTime();
  const notBefore = new Date();
  const notAfter = new Date();
  notAfter.setFullYear(notAfter.getFullYear() + 1);

  // create new certificate
  const cert = new KJUR.asn1.x509.Certificate({
    serial: { hex: randomBytes(16).toString("hex") },
    issuer: caCert.getSubject(), // issuer from CA
    subject: csr.subject, // subject from CSR
    notbefore: time.formatDate(notBefore),
    notafter: time.formatDate(notAfter),
    sbjpubkey: csr.sbjpubkey,
    cakey: caKey,
    sigalg: "SHA256withECDSA",
  });

  // get signed certificate
  const certPem = cert.getPEM();
  const certPemNormalized = certPem.replace(/[\r]+/g, "");

  console.log("Certificate created.");

  return certPemNormalized;
}
