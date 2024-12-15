#pragma once 

#include "./utils.h"

namespace AltAI
{
    template <typename T>
        void writeVector(FDataStreamBase* pStream, const std::vector<T>& v)
    {
        const size_t size = v.size();
        pStream->Write(size);
        for (size_t i = 0; i < size; ++i)
        {
            pStream->Write(v[i]);
        }
    }

    template <typename T, typename U>
        void writePairVector(FDataStreamBase* pStream, const std::vector<std::pair<T, U> >& v)
    {
        const size_t size = v.size();
        pStream->Write(size);
        for (size_t i = 0; i < size; ++i)
        {
            pStream->Write(v[i].first);
            pStream->Write(v[i].second);
        }
    }

    template <typename T>
        void writeComplexVector(FDataStreamBase* pStream, const std::vector<T>& v)
    {
        const size_t size = v.size();
        pStream->Write(size);
        for (size_t i = 0; i < size; ++i)
        {
            v[i].write(pStream);
        }
    }

    template <int N, typename T>
        void writeArray(FDataStreamBase* pStream, const boost::array<T, N>& data)
    {
        for (size_t i = 0; i < N; ++i)
        {
            pStream->Write(data[i]);
        }
    }

    template <typename T, typename C>
        void readVector(FDataStreamBase* pStream, std::vector<T>& v)
    {
        size_t size;
        pStream->Read(&size);
        v.clear();

        for (size_t i = 0; i < size; ++i)
        {
            T value;
            pStream->Read((C*)&value);
            v.push_back(value);
        }
    }

    template <typename T, typename U, typename C>
        void readPairVector(FDataStreamBase* pStream, std::vector<std::pair<T, U> >& v)
    {
        size_t size;
        pStream->Read(&size);
        v.clear();

        for (size_t i = 0; i < size; ++i)
        {
            T first;
            pStream->Read((C*)&first);
            U second;
            pStream->Read(&second);
            v.push_back(std::make_pair(first, second));
        }
    }

    template <typename T>
        void readComplexVector(FDataStreamBase* pStream, std::vector<T>& v)
    {
        size_t size;
        pStream->Read(&size);
        v.clear();

        for (size_t i = 0; i < size; ++i)
        {
            T value;
            value.read(pStream);
            v.push_back(value);
        }
    }

    template <int N, typename T>
        void readArray(FDataStreamBase* pStream, boost::array<T, N>& data)
    {
        for (size_t i = 0; i < N; ++i)
        {
            pStream->Read(&data[i]);
        }
    }

    template <typename T>
        void writeList(FDataStreamBase* pStream, const std::list<T>& l)
    {
        const size_t size = l.size();
        pStream->Write(size);
        for (std::list<T>::const_iterator ci(l.begin()), ciEnd(l.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(*ci);
        }
    }

    template <typename T>
        void writeComplexList(FDataStreamBase* pStream, const std::list<T>& l)
    {
        const size_t size = l.size();
        pStream->Write(size);
        for (std::list<T>::const_iterator ci(l.begin()), ciEnd(l.end()); ci != ciEnd; ++ci)
        {
            ci->write(pStream);
        }
    }

    template <typename T, typename C>
        void readList(FDataStreamBase* pStream, std::list<T>& l)
    {
        size_t size;
        pStream->Read(&size);
        l.clear();

        for (size_t i = 0; i < size; ++i)
        {
            T value;
            pStream->Read((C*)&value);
            l.push_back(value);
        }
    }

    template <typename T>
        void readComplexList(FDataStreamBase* pStream, std::list<T>& l)
    {
        size_t size;
        pStream->Read(&size);
        l.clear();

        for (size_t i = 0; i < size; ++i)
        {
            T value;
            value.read(pStream);
            l.push_back(value);
        }
    }

    template <typename T>
        void writeSet(FDataStreamBase* pStream, const std::set<T>& s)
    {
        const size_t size = s.size();
        pStream->Write(size);
        for (std::set<T>::const_iterator ci(s.begin()), ciEnd(s.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(*ci);
        }
    }

    template <typename T>
        void writeComplexSet(FDataStreamBase* pStream, const std::set<T>& s)
    {
        const size_t size = s.size();
        pStream->Write(size);
        for (std::set<T>::const_iterator ci(s.begin()), ciEnd(s.end()); ci != ciEnd; ++ci)
        {
            ci->write(pStream);
        }
    }

    template <typename T, typename C>
        void readSet(FDataStreamBase* pStream, std::set<T>& s)
    {
        size_t size;
        pStream->Read(&size);
        s.clear();

        for (size_t i = 0; i < size; ++i)
        {
            T value;
            pStream->Read((C*)&value);
            s.insert(value);
        }
    }

    template <typename T>
        void readComplexSet(FDataStreamBase* pStream, std::set<T>& s)
    {
        size_t size;
        pStream->Read(&size);
        s.clear();

        for (size_t i = 0; i < size; ++i)
        {
            T value;
            value.read(pStream);
            s.insert(value);
        }
    }

    template <typename K, typename T>
        void writeMap(FDataStreamBase* pStream, const std::map<K, T>& m)
    {
        const size_t size = m.size();
        pStream->Write(size);
        for (std::map<K, T>::const_iterator ci(m.begin()), ciEnd(m.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            pStream->Write(ci->second);
        }
    }

    template <typename K, typename T>
        void writeComplexKeyMap(FDataStreamBase* pStream, const std::map<K, T>& m)
    {
        const size_t size = m.size();
        pStream->Write(size);
        for (std::map<K, T>::const_iterator ci(m.begin()), ciEnd(m.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            pStream->Write(ci->second);
        }
    }

    template <typename K, typename T>
        void writeComplexValueMap(FDataStreamBase* pStream, const std::map<K, T>& m)
    {
        const size_t size = m.size();
        pStream->Write(size);
        for (std::map<K, T>::const_iterator ci(m.begin()), ciEnd(m.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            ci->second.write(pStream);            
        }
    }

    template <typename K, typename T>
        void writeComplexMap(FDataStreamBase* pStream, const std::map<K, T>& m)
    {
        const size_t size = m.size();
        pStream->Write(size);
        for (std::map<K, T>::const_iterator ci(m.begin()), ciEnd(m.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            ci->second.write(pStream);
        }
    }

    template <typename K, typename T>
        void writeMultiMap(FDataStreamBase* pStream, const std::multimap<K, T>& m)
    {
        const size_t size = m.size();
        pStream->Write(size);
        for (std::multimap<K, T>::const_iterator ci(m.begin()), ciEnd(m.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            pStream->Write(ci->second);
        }
    }

    template <typename K, typename T, typename C1, typename C2>
        void readMap(FDataStreamBase* pStream, std::map<K, T>& m)
    {
        size_t size;
        pStream->Read(&size);
        m.clear();

        for (size_t i = 0; i < size; ++i)
        {
            K key;
            T value;
            pStream->Read((C1*)&key);
            pStream->Read((C2*)&value);
            m.insert(std::make_pair(key, value));
        }
    }

    template <typename K, typename T, typename C>
        void readComplexKeyMap(FDataStreamBase* pStream, std::map<K, T>& m)
    {
        size_t size;
        pStream->Read(&size);
        m.clear();

        for (size_t i = 0; i < size; ++i)
        {
            K key;
            T value;

            key.read(pStream);
            pStream->Read((C*)&value);
            m.insert(std::make_pair(key, value));
        }
    }

    template <typename K, typename T, typename C>
        void readComplexValueMap(FDataStreamBase* pStream, std::map<K, T>& m)
    {
        size_t size;
        pStream->Read(&size);
        m.clear();

        for (size_t i = 0; i < size; ++i)
        {
            K key;
            T value;

            pStream->Read((C*)&key);
            value.read(pStream);            
            m.insert(std::make_pair(key, value));
        }
    }

    template <typename K, typename T>
        void readComplexMap(FDataStreamBase* pStream, std::map<K, T>& m)
    {
        size_t size;
        pStream->Read(&size);
        m.clear();

        for (size_t i = 0; i < size; ++i)
        {
            K key;
            T value;

            key.read(pStream);
            value.read(pStream);
            m.insert(std::make_pair(key, value));
        }
    }

    template <typename K, typename T, typename C1, typename C2>
        void readMultiMap(FDataStreamBase* pStream, std::multimap<K, T>& m)
    {
        size_t size;
        pStream->Read(&size);
        m.clear();

        for (size_t i = 0; i < size; ++i)
        {
            K key;
            T value;
            pStream->Read((C1*)&key);
            pStream->Read((C2*)&value);
            m.insert(std::make_pair(key, value));
        }
    }
}