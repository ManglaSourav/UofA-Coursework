package edu.arizona.cs;

public class Document {
    String docid;
    public Document(String docid_in){
        docid=docid_in;

    }

    public boolean equals(Document answer) {
        if  (!docid.equals(answer.docid))
        {
            return false;
        }
        return true;
    }
}